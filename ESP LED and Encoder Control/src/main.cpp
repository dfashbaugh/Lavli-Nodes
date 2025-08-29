#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Encoder.h>

#define LED_PIN GPIO_NUM_8
#define LED_COUNT 24
#define ENCODER_A GPIO_NUM_5
#define ENCODER_B GPIO_NUM_6
#define ENCODER_SWITCH GPIO_NUM_7
#define DEBOUNCE_DELAY 50

enum Mode {
  MODE_RED = 0,
  MODE_GREEN = 1,
  MODE_BLUE = 2,
  MODE_LED_MOVEMENT = 3,
  MODE_BLUE_TRAIL = 4
};

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP32Encoder encoder;

int lastEncoderValue = 0;
int brightness = 50;
Mode currentMode = MODE_RED;
int activeLED = 0;

bool lastSwitchState = HIGH;
unsigned long lastDebounceTime = 0;

void setup() {
  Serial.begin(115200);
  
  strip.begin();
  strip.show();
  strip.setBrightness(brightness);
  
  ESP32Encoder::useInternalWeakPullResistors = puType::UP;
  encoder.attachHalfQuad(ENCODER_A, ENCODER_B);
  encoder.clearCount();

  pinMode(ENCODER_SWITCH, INPUT);

  for(int i = 0; i < LED_COUNT; i++){
    strip.setPixelColor(i, strip.Color(255, 0, 0));
  }
  strip.show();
  
  Serial.println("ESP32-S3 NeoPixel + Encoder Control Ready");
  Serial.println("Pins: LED=8, Encoder A=5, B=6");
}

uint32_t getModeColor() {
  switch (currentMode) {
    case MODE_RED:
      return strip.Color(255, 0, 0);
    case MODE_GREEN:
      return strip.Color(0, 255, 0);
    case MODE_BLUE:
      return strip.Color(0, 0, 255);
    default:
      return strip.Color(255, 255, 255);
  }
}

void updateLEDs() {
  if (currentMode == MODE_LED_MOVEMENT) {
    strip.clear();
    strip.setPixelColor(activeLED, strip.Color(255, 255, 255));
  } else if (currentMode == MODE_BLUE_TRAIL) {
    for (int i = 0; i <= activeLED; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 255));
    }
    for (int i = activeLED + 1; i < LED_COUNT; i++) {
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  } else {
    uint32_t color = getModeColor();
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, color);
    }
  }
  strip.show();
}

void handleSwitchPress() {
  bool switchReading = digitalRead(ENCODER_SWITCH);
  
  // if (switchReading != lastSwitchState) {
  //   lastDebounceTime = millis();
  // }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (switchReading != lastSwitchState) {
      if (switchReading == LOW) {
        currentMode = (Mode)((currentMode + 1) % 5);
        
        Serial.print("Mode changed to: ");
        Serial.println(currentMode);
        
        if (currentMode == MODE_LED_MOVEMENT || currentMode == MODE_BLUE_TRAIL) {
          activeLED = 0;
        }
        
        updateLEDs();
      }
      lastSwitchState = switchReading;
    }

    lastDebounceTime = millis();
  }
}

void loop() {
  handleSwitchPress();
  
  int encoderValue = encoder.getCount();
  
  if (encoderValue != lastEncoderValue) {
    int diff = encoderValue - lastEncoderValue;
    
    if (currentMode == MODE_LED_MOVEMENT || currentMode == MODE_BLUE_TRAIL) {
      activeLED = constrain(activeLED + diff, 0, LED_COUNT - 1);
      Serial.print("Active LED: ");
      Serial.println(activeLED);
    } else {
      brightness = constrain(brightness + (diff * 5), 0, 255);
      strip.setBrightness(brightness);
      Serial.print("Encoder: ");
      Serial.print(encoderValue);
      Serial.print(", Brightness: ");
      Serial.println(brightness);
    }
    
    updateLEDs();
    lastEncoderValue = encoderValue;
  }
  
  delay(10);
}