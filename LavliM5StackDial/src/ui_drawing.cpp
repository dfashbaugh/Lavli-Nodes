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

void drawRecycleSymbol(int cx, int cy, int r, uint32_t color) {
  // Draw 3 curved arrows in a triangular formation (recycle symbol)
  // Simplified as 3 triangular arrows pointing clockwise around center

  // Arrow 1 (top) - pointing right
  int arrow1_x = cx;
  int arrow1_y = cy - r;
  M5Dial.Display.fillTriangle(
    arrow1_x - r/2, arrow1_y - r/3,
    arrow1_x + r/2, arrow1_y - r/3,
    arrow1_x + r/2, arrow1_y + r/3,
    color);
  M5Dial.Display.fillTriangle(
    arrow1_x + r/2, arrow1_y - r/2,
    arrow1_x + r/2 + r/3, arrow1_y,
    arrow1_x + r/2, arrow1_y + r/2,
    color);

  // Arrow 2 (bottom-left) - pointing up-right
  int arrow2_x = cx - r * 0.866; // cos(60°) * r
  int arrow2_y = cy + r * 0.5;   // sin(60°) * r
  M5Dial.Display.fillTriangle(
    arrow2_x - r/3, arrow2_y + r/2,
    arrow2_x + r/3, arrow2_y + r/2,
    arrow2_x, arrow2_y - r/2,
    color);
  M5Dial.Display.fillTriangle(
    arrow2_x - r/2, arrow2_y - r/2,
    arrow2_x, arrow2_y - r/2 - r/3,
    arrow2_x + r/2, arrow2_y - r/2,
    color);

  // Arrow 3 (bottom-right) - pointing up-left
  int arrow3_x = cx + r * 0.866;
  int arrow3_y = cy + r * 0.5;
  M5Dial.Display.fillTriangle(
    arrow3_x - r/3, arrow3_y + r/2,
    arrow3_x + r/3, arrow3_y + r/2,
    arrow3_x, arrow3_y - r/2,
    color);
  M5Dial.Display.fillTriangle(
    arrow3_x - r/2, arrow3_y - r/2,
    arrow3_x, arrow3_y - r/2 - r/3,
    arrow3_x + r/2, arrow3_y - r/2,
    color);
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

void drawWashingScreen(unsigned long startTime) {
  auto& d = M5Dial.Display;
  d.clear();

  // Get screen center
  int cx = d.width() / 2;
  int cy = d.height() / 2 - 20;
  int iconR = 30;

  // Draw water drop icon
  uint32_t c = d.color888(40, 200, 255);
  drawWaterDrop(cx, cy, iconR, c);

  // Main text
  d.setTextDatum(middle_center);
  d.setTextSize(2);
  d.setTextColor(COLOR_WHITE, COLOR_BLACK);
  d.drawString("Washing", cx, cy + iconR + 20);

  // Calculate elapsed time
  unsigned long elapsed = (millis() - startTime) / 1000;  // seconds
  unsigned long minutes = elapsed / 60;
  unsigned long seconds = elapsed % 60;

  // Format and display elapsed time
  char timeStr[10];
  sprintf(timeStr, "%02lu:%02lu", minutes, seconds);
  d.setTextSize(2);
  d.setTextColor(COLOR_CYAN, COLOR_BLACK);
  d.drawString(timeStr, cx, cy + iconR + 50);

  // Hint text
  d.setTextSize(1);
  d.setTextColor(COLOR_LIGHT_GRAY, COLOR_BLACK);
  d.drawString("Press button to stop", cx, d.height() - 18);

  // Draw status icons
  drawStatusIcons();
}

void drawDryingScreen(unsigned long startTime) {
  auto& d = M5Dial.Display;
  d.clear();

  // Get screen center
  int cx = d.width() / 2;
  int cy = d.height() / 2 - 20;
  int iconR = 30;

  // Draw sun icon
  uint32_t c = d.color888(255, 210, 40);
  drawSun(cx, cy, iconR, c);

  // Main text
  d.setTextDatum(middle_center);
  d.setTextSize(2);
  d.setTextColor(COLOR_WHITE, COLOR_BLACK);
  d.drawString("Drying", cx, cy + iconR + 20);

  // Calculate elapsed time
  unsigned long elapsed = (millis() - startTime) / 1000;  // seconds
  unsigned long minutes = elapsed / 60;
  unsigned long seconds = elapsed % 60;

  // Format and display elapsed time
  char timeStr[10];
  sprintf(timeStr, "%02lu:%02lu", minutes, seconds);
  d.setTextSize(2);
  d.setTextColor(COLOR_CYAN, COLOR_BLACK);
  d.drawString(timeStr, cx, cy + iconR + 50);

  // Hint text
  d.setTextSize(1);
  d.setTextColor(COLOR_LIGHT_GRAY, COLOR_BLACK);
  d.drawString("Press button to stop", cx, d.height() - 18);

  // Draw status icons
  drawStatusIcons();
}

void drawRecyclingScreen(unsigned long startTime) {
  auto& d = M5Dial.Display;
  d.clear();

  // Get screen center
  int cx = d.width() / 2;
  int cy = d.height() / 2 - 20;
  int iconR = 30;

  // Draw recycle symbol (green)
  uint32_t c = d.color888(40, 255, 100);
  drawRecycleSymbol(cx, cy, iconR, c);

  // Main text
  d.setTextDatum(middle_center);
  d.setTextSize(2);
  d.setTextColor(COLOR_WHITE, COLOR_BLACK);
  d.drawString("Recycling", cx, cy + iconR + 20);

  // Calculate elapsed time
  unsigned long elapsed = (millis() - startTime) / 1000;  // seconds
  unsigned long minutes = elapsed / 60;
  unsigned long seconds = elapsed % 60;

  // Format and display elapsed time
  char timeStr[10];
  sprintf(timeStr, "%02lu:%02lu", minutes, seconds);
  d.setTextSize(2);
  d.setTextColor(COLOR_CYAN, COLOR_BLACK);
  d.drawString(timeStr, cx, cy + iconR + 50);

  // Hint text
  d.setTextSize(1);
  d.setTextColor(COLOR_LIGHT_GRAY, COLOR_BLACK);
  d.drawString("Press button to stop", cx, d.height() - 18);

  // Draw status icons
  drawStatusIcons();
}

void drawOptionsScreen() {
  auto& d = M5Dial.Display;
  d.clear();

  // Get screen center
  int cx = d.width() / 2;
  int cy = d.height() / 2 - 10;

  // Main text
  d.setTextDatum(middle_center);
  d.setTextSize(3);
  d.setTextColor(COLOR_WHITE, COLOR_BLACK);
  d.drawString("*", cx, cy - 20);  // Gear placeholder

  d.setTextSize(2);
  d.drawString("Options", cx, cy + 20);

  d.setTextSize(1);
  d.setTextColor(COLOR_LIGHT_GRAY, COLOR_BLACK);
  d.drawString("Coming Soon", cx, cy + 50);

  // Hint text
  d.setTextSize(1);
  d.setTextColor(COLOR_LIGHT_GRAY, COLOR_BLACK);
  d.drawString("Press to return", cx, d.height() - 18);

  // Draw status icons
  drawStatusIcons();
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
  } else if (m == MODE_DRY) {
    // sun: yellow
    uint32_t c = d.color888(255, 210, 40);
    drawSun(cx, cy, iconR, c);
    d.drawString("Dry", cx, cy + iconR + 28);
  } else if (m == MODE_RECYCLE) {
    // recycle: green
    uint32_t c = d.color888(40, 255, 100);
    drawRecycleSymbol(cx, cy, iconR, c);
    d.drawString("Recycle", cx, cy + iconR + 28);
  } else if (m == MODE_OPTIONS) {
    // gear/settings icon: white
    d.setTextSize(3);
    d.drawString("*", cx, cy);  // Gear placeholder (use * for now)
    d.setTextSize(2);
    d.drawString("Options", cx, cy + iconR + 28);
  }

  // hint text
  d.setTextSize(1);
  d.setTextColor(COLOR_LIGHT_GRAY, COLOR_BLACK);
  d.drawString("Turn dial • Press to select", cx, d.height() - 18);

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