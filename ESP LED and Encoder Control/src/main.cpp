#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Encoder.h>

#define LED_PIN GPIO_NUM_8
#define LED_COUNT 24
#define ENCODER_A GPIO_NUM_5
#define ENCODER_B GPIO_NUM_6
#define ENCODER_SWITCH GPIO_NUM_7

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP32Encoder encoder;

int lastEncoderValue = 0;
int brightness = 50;
uint32_t currentColor = strip.Color(255, 0, 0);

void setup() {
  Serial.begin(115200);
  
  strip.begin();
  strip.show();
  strip.setBrightness(brightness);
  
  ESP32Encoder::useInternalWeakPullResistors = puType::UP;
  encoder.attachHalfQuad(ENCODER_A, ENCODER_B);
  encoder.clearCount();

  pinMode(ENCODER_SWITCH, INPUT_PULLUP);
  
  Serial.println("ESP32-S3 NeoPixel + Encoder Control Ready");
  Serial.println("Pins: LED=8, Encoder A=5, B=6");
}

void loop() {
  int encoderValue = encoder.getCount();
  
  if (encoderValue != lastEncoderValue) {
    int diff = encoderValue - lastEncoderValue;
    brightness = constrain(brightness + (diff * 5), 0, 255);
    
    strip.setBrightness(brightness);
    for (int i = 0; i < LED_COUNT; i++) {
      strip.setPixelColor(i, currentColor);
    }
    strip.show();
    
    Serial.print("Encoder: ");
    Serial.print(encoderValue);
    Serial.print(", Brightness: ");
    Serial.println(brightness);
    
    lastEncoderValue = encoderValue;
  }
  
  delay(10);
}