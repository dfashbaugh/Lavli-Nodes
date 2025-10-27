#include <M5Dial.h>

enum Mode : int { MODE_WASH = 0, MODE_DRY = 1 };

long lastEnc = LONG_MIN;
Mode currentMode = MODE_WASH;

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
}

void setup() {
  auto cfg = M5.config();
  // enableEncoder = true, enableRFID = false
  M5Dial.begin(cfg, true, false);              // initializes device and encoder
  M5Dial.Display.setTextDatum(middle_center);  // center text
  drawUI(currentMode);

  // Start encoder at 0 so modulo works cleanly
  M5Dial.Encoder.write(0);
  lastEnc = 0;
}

void loop() {
  M5Dial.update();  // required for input updates

  long enc = M5Dial.Encoder.read();
  if (enc != lastEnc) {
    // Reduce to two states using modulo; keeps working if you keep spinning
    int idx = (int)((enc % 2 + 2) % 2);  // 0 or 1, handles negatives
    Mode newMode = (idx == 0) ? MODE_WASH : MODE_DRY;
    if (newMode != currentMode) {
      currentMode = newMode;
      M5Dial.Speaker.tone(2400, 30);  // little tick
      drawUI(currentMode);
    }
    lastEnc = enc;
  }
}
