#include "ui_drawing.h"
#include "provisioning.h"

// Icon drawing functions
void drawWaterDrop(int cx, int cy, int r, uint32_t color) {
  // simple teardrop: circle + triangle "tip"
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

// Status and UI functions
void drawStatusIcons() {
  auto& d = M5Dial.Display;
  int screenWidth = d.width();
  
  // Draw WiFi icon if connected
  if (wifiConnected) {
    drawWiFiIcon(screenWidth - 25, 5, COLOR_GREEN); // Green for connected
  }
  
  // Draw Bluetooth icon if BLE is active
  if (bleActive) {
    drawBluetoothIcon(screenWidth - 45, 5, COLOR_BLUE); // Blue for BLE active
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
  
  // Main text with gradient effect
  d.setTextColor(COLOR_CYAN); // Cyan
  d.drawString("Lavli", cx, cy);
}

void drawWashingScreen() {
  auto& d = M5Dial.Display;
  d.clear();
  
  // Get screen center
  int cx = d.width() / 2;
  int cy = d.height() / 2;
  
  // Create a shiny gradient effect with multiple text layers
  d.setTextDatum(middle_center);
  d.setTextSize(3);
  
  // Main text with gradient effect
  d.setTextColor(COLOR_CYAN); // Cyan
  d.drawString("Washing", cx, cy);
}

void drawDryingScreen() {
  auto& d = M5Dial.Display;
  d.clear();
  
  // Get screen center
  int cx = d.width() / 2;
  int cy = d.height() / 2;
  
  // Create a shiny gradient effect with multiple text layers
  d.setTextDatum(middle_center);
  d.setTextSize(3);
  
  // Main text with gradient effect
  d.setTextColor(COLOR_CYAN); // Cyan
  d.drawString("Drying", cx, cy);
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
  d.setTextColor(COLOR_WHITE, COLOR_BLACK);

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
  d.setTextColor(COLOR_LIGHT_GRAY, COLOR_BLACK);
  d.drawString("Turn dial to switch", cx, d.height() - 18);
  
  // Draw status icons
  drawStatusIcons();
}

// Utility functions
void clearScreen() {
  M5Dial.Display.clear();
}

void setTextDefaults() {
  M5Dial.Display.setTextDatum(middle_center);
  M5Dial.Display.setTextColor(COLOR_WHITE, COLOR_BLACK);
}