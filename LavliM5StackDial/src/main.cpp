#include <M5Dial.h>
#include "can_comm.h"
#include "provisioning.h"

enum Mode : int { MODE_WASH = 0, MODE_DRY = 1 };
enum Screen : int { SCREEN_STARTUP = 0, SCREEN_MAIN = 1 };

long lastEnc = LONG_MIN;
Mode currentMode = MODE_WASH;
Mode lastSentMode = MODE_DRY; // Initialize to opposite so first selection sends command
Screen currentScreen = SCREEN_STARTUP;
unsigned long startupStartTime = 0;
const unsigned long STARTUP_DURATION = 3000; // 3 seconds

// CAN communication variables
bool canInitialized = false;

// ---------- Draw helpers ----------
void drawWaterDrop(int cx, int cy, int r, uint32_t color) {
  // simple teardrop: circle + triangle “tip”
  M5Dial.Display.fillCircle(cx, cy, r, color);
  // triangle tip on top
  int tipH = r + r / 2;
  int x0 = cx, y0 = cy - r - tipH;
  int x1 = cx - r, y1 = cy - r / 4;
  int x2 = cx + r, y2 = y1;
  M5Dial.Display.fillTriangle(x0, y0, x1, y1, x2, y2, color);
}

void drawSun(int cx, int cy, int r, uint32_t color) {
  M5Dial.Display.fillCircle(cx, cy, r, color);
  // rays
  int rayLen = r + r / 2;
  for (int i = 0; i < 12; ++i) {
    float a = i * (PI / 6.0f);
    int x1 = cx + (int)(cosf(a) * (r + 2));
    int y1 = cy + (int)(sinf(a) * (r + 2));
    int x2 = cx + (int)(cosf(a) * (rayLen));
    int y2 = cy + (int)(sinf(a) * (rayLen));
    M5Dial.Display.drawLine(x1, y1, x2, y2, color);
  }
}

void drawWiFiIcon(int x, int y, uint32_t color) {
  auto& d = M5Dial.Display;
  // Draw WiFi signal bars
  d.fillRect(x, y + 6, 2, 2, color);
  d.fillRect(x + 3, y + 4, 2, 4, color);
  d.fillRect(x + 6, y + 2, 2, 6, color);
  d.fillRect(x + 9, y, 2, 8, color);
}

void drawBluetoothIcon(int x, int y, uint32_t color) {
  auto& d = M5Dial.Display;
  // Simple Bluetooth "B" shape
  d.fillRect(x + 2, y, 1, 8, color);       // Vertical line
  d.fillRect(x + 3, y, 3, 1, color);       // Top horizontal
  d.fillRect(x + 3, y + 3, 3, 1, color);   // Middle horizontal  
  d.fillRect(x + 3, y + 7, 3, 1, color);   // Bottom horizontal
  d.fillRect(x + 6, y + 1, 1, 2, color);   // Top right vertical
  d.fillRect(x + 6, y + 5, 1, 2, color);   // Bottom right vertical
}

void drawStatusIcons() {
  auto& d = M5Dial.Display;
  int screenWidth = d.width();
  
  // Draw WiFi icon if connected
  if (wifiConnected) {
    drawWiFiIcon(screenWidth - 25, 5, 0x07E0); // Green for connected
  }
  
  // Draw Bluetooth icon if BLE is active
  if (bleActive) {
    drawBluetoothIcon(screenWidth - 45, 5, 0x001F); // Blue for BLE active
  }
}

void drawStartupScreen() {
  auto& d = M5Dial.Display;
  d.clear();
  
  // Get screen center
  int cx = d.width() / 2;
  int cy = d.height() / 2;
  
  // Create a shiny gradient effect with multiple text layers
  d.setTextDatum(middle_center);
  d.setTextSize(3);
  
  // Background shadow (slightly offset)
  // d.setTextColor(0x2104); // Dark blue shadow
  // d.drawString("Lavli", cx + 2, cy + 2);
  
  // Main text with gradient effect
  d.setTextColor(0x07FF); // Cyan
  d.drawString("Lavli", cx, cy);
  
  // // Highlight overlay
  // d.setTextColor(WHITE);
  // d.setTextSize(2);
  // d.drawString("Lavli", cx - 1, cy - 1);
}

void drawUI(Mode m) {
  auto& d = M5Dial.Display;
  d.clear();

  // Layout
  int cx = d.width() / 2;
  int cy = d.height() / 2 - 10;
  int iconR = 40;

  // Title
  d.setTextDatum(middle_center);
  d.setTextSize(2);
  d.setTextColor(WHITE, BLACK);

  if (m == MODE_WASH) {
    // water: cyan
    uint32_t c = d.color888(40, 200, 255);
    drawWaterDrop(cx, cy, iconR, c);
    d.drawString("Wash", cx, cy + iconR + 28);
  } else {
    // sun: yellow
    uint32_t c = d.color888(255, 210, 40);
    drawSun(cx, cy, iconR, c);
    d.drawString("Dry", cx, cy + iconR + 28);
  }

  // hint text
  d.setTextSize(1);
  d.setTextColor(0xC618 /* light gray */, BLACK);
  d.drawString("Turn dial to switch", cx, d.height() - 18);
  
  // Draw status icons
  drawStatusIcons();
}

void setup() {
  auto cfg = M5.config();
  // enableEncoder = true, enableRFID = false
  M5Dial.begin(cfg, true, false);              // initializes device and encoder
  M5Dial.Display.setTextDatum(middle_center);  // center text
  
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000);
  Serial.println("M5Stack Dial - Lavli Controller Starting...");
  
  // Show startup screen
  drawStartupScreen();
  startupStartTime = millis();

  // Initialize CAN communication
  canInitialized = initializeCAN();
  if (canInitialized) {
    Serial.println("CAN communication ready");
  } else {
    Serial.println("CAN communication failed to initialize");
  }

  // Initialize provisioning
  if (USE_BLE_PROVISIONING) {
    Serial.println("Initializing BLE provisioning...");
    initializeBLE();
  }
  
  // Try to connect to WiFi
  Serial.println("Setting up WiFi...");
  setupWiFi();

  // Start encoder at 0 so modulo works cleanly
  M5Dial.Encoder.write(0);
  lastEnc = 0;
}

void loop() {
  M5Dial.update();  // required for input updates

  // Handle startup screen timing
  if (currentScreen == SCREEN_STARTUP) {
    if (millis() - startupStartTime >= STARTUP_DURATION) {
      currentScreen = SCREEN_MAIN;
      drawUI(currentMode);
    }
    return; // Don't process encoder during startup
  }

  // Process provisioning
  processProvisioning();
  
  // Process CAN messages if initialized
  if (canInitialized) {
    processCANMessages();
  }

  // Main screen encoder handling
  long enc = M5Dial.Encoder.read();
  if (enc != lastEnc) {
    // Require 4 encoder counts to switch modes (better detent alignment)
    int idx = (int)((enc / 4 % 2 + 2) % 2);  // 0 or 1, handles negatives, divides by 4
    Mode newMode = (idx == 0) ? MODE_WASH : MODE_DRY;
    if (newMode != currentMode) {
      currentMode = newMode;
      M5Dial.Speaker.tone(2400, 30);  // little tick
      drawUI(currentMode);
      
      // Send CAN command if mode changed and CAN is initialized
      if (canInitialized && newMode != lastSentMode) {
        if (newMode == MODE_WASH) {
          sendWashCommand();
        } else {
          sendDryCommand();
        }
        lastSentMode = newMode;
      }
    }
    lastEnc = enc;
  }
  
  // Handle button press to send stop command
  if (M5Dial.BtnA.wasPressed()) {
    M5Dial.Speaker.tone(1800, 50);  // Different tone for button press
    if (canInitialized) {
      Serial.println("Button pressed - sending STOP command");
      sendStopCommand();
    }
  }
}
