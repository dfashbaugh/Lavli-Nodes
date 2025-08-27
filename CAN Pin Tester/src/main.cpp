#include <Arduino.h>
#include <driver/twai.h>

// CAN pins - using valid ESP32-S3 GPIO pins
#define CAN_TX_PIN GPIO_NUM_4
#define CAN_RX_PIN GPIO_NUM_5


void setup() {
  Serial.begin(115200);

  pinMode(CAN_TX_PIN, OUTPUT);

  for(int i = 0; i < 10; i++){
    delay(500);
    Serial.println("hello");
  }
  

}

void loop() {
  // Serial.println("loop");

  digitalWrite(CAN_TX_PIN, HIGH);
  delay(1);
  digitalWrite(CAN_TX_PIN, LOW);
  delay(1);

}
