#ifndef UI_DRAWING_H
#define UI_DRAWING_H

#include <Arduino.h>
#include <M5Dial.h>

// Mode enum (shared with main)
enum Mode : int { MODE_WASH = 0, MODE_DRY = 1, MODE_RECYCLE = 2, MODE_OPTIONS = 3 };

// Screen enum (shared with main)
enum Screen : int {
    SCREEN_STARTUP = 0,
    SCREEN_MAIN = 1,
    SCREEN_OPTIONS = 2,
    SCREEN_WASHING = 3,
    SCREEN_DRYING = 4,
    SCREEN_RECYCLING = 5
};

// Color definitions
#define COLOR_WHITE     0xFFFF
#define COLOR_BLACK     0x0000
#define COLOR_CYAN      0x07FF
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_LIGHT_GRAY 0xC618

// Icon drawing functions
void drawWaterDrop(int cx, int cy, int r, uint32_t color);
void drawSun(int cx, int cy, int r, uint32_t color);
void drawRecycleSymbol(int cx, int cy, int r, uint32_t color);
void drawWiFiIcon(int x, int y, uint32_t color);
void drawBluetoothIcon(int x, int y, uint32_t color);

// Status and UI functions
void drawStatusIcons();
void drawStartupScreen();
void drawUI(Mode m);
void drawOptionsScreen();
void drawWashingScreen(unsigned long startTime);
void drawDryingScreen(unsigned long startTime);
void drawRecyclingScreen(unsigned long startTime);

// Utility functions
void clearScreen();
void setTextDefaults();

#endif // UI_DRAWING_H