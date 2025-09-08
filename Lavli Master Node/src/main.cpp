#include <Arduino.h>
#include <driver/twai.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Encoder.h>


// Interface Setup
#define LED_PIN GPIO_NUM_8
#define LED_COUNT 24
#define ENCODER_A GPIO_NUM_5
#define ENCODER_B GPIO_NUM_6
#define ENCODER_SWITCH GPIO_NUM_7
#define DEBOUNCE_DELAY 50
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP32Encoder encoder;

int lastEncoderValue = 0;
int brightness = 50;
bool lastSwitchState = HIGH;
unsigned long lastDebounceTime = 0;

// CAN IDs of Controllers
#define CONTROLLER_120V_ADDRESS 0x543
#define CONTROLLER_MOTOR_ADDRESS 0x311


// CAN pins
#define CAN_TX_PIN GPIO_NUM_4
#define CAN_RX_PIN GPIO_NUM_5

// Command definitions for output control
#define ACTIVATE_CMD   0x01
#define DEACTIVATE_CMD 0x02

// New command definitions for sensor requests
#define READ_ANALOG_CMD   0x03
#define READ_DIGITAL_CMD  0x04
#define READ_ALL_ANALOG_CMD 0x05
#define READ_ALL_DIGITAL_CMD 0x06

// Response command definitions
#define ACK_ACTIVATE      0x10
#define ACK_DEACTIVATE    0x11
#define ANALOG_DATA       0x20
#define DIGITAL_DATA      0x21
#define ALL_ANALOG_DATA   0x22
#define ALL_DIGITAL_DATA  0x23
#define ERROR_RESPONSE    0xFF

// Sensor data structure for storing received values
struct SensorReading {
  uint16_t analog_value;
  bool digital_value;
  bool valid;
  unsigned long timestamp;
};

// Storage for sensor readings from different devices
#define MAX_DEVICES 16
#define MAX_PINS 8
SensorReading analog_readings[MAX_DEVICES][MAX_PINS];
SensorReading digital_readings[MAX_DEVICES][MAX_PINS];

// CAN configuration
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// Function prototypes
bool initializeCAN();
void initializeSensorStorage();
bool sendOutputCommand(uint16_t device_address, uint8_t command, uint8_t port);
bool requestAnalogReading(uint16_t device_address, uint8_t pin);
bool requestDigitalReading(uint16_t device_address, uint8_t pin);
bool requestAllAnalogReadings(uint16_t device_address);
bool requestAllDigitalReadings(uint16_t device_address);
void receiveCANMessages();
void processReceivedMessage(twai_message_t* message);
void storeSensorReading(uint16_t device_address, uint8_t pin, uint16_t value, bool is_analog);
SensorReading getAnalogReading(uint16_t device_address, uint8_t pin);
SensorReading getDigitalReading(uint16_t device_address, uint8_t pin);
void printSensorData(uint16_t device_address);
uint8_t getDeviceIndex(uint16_t device_address);

void setupInterface()
{
  strip.begin();
  strip.show();
  strip.setBrightness(brightness);
  
  ESP32Encoder::useInternalWeakPullResistors = puType::UP;
  encoder.attachHalfQuad(ENCODER_A, ENCODER_B);
  encoder.clearCount();

  pinMode(ENCODER_SWITCH, INPUT_PULLUP);

  for(int i = 0; i < LED_COUNT; i++){
    strip.setPixelColor(i, strip.Color(0, 0, 255));
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
        // TODO: Do Switch functions in here
      }
      lastSwitchState = switchReading;
    }

    lastDebounceTime = millis();
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("CAN Master Node with Sensor Support");
  
  delay(2000);

  setupInterface();
  
  // Initialize sensor data storage
  initializeSensorStorage();
  
  // Initialize CAN
  if (initializeCAN()) {
    Serial.println("CAN initialized successfully");
    Serial.println("Commands:");
    Serial.println("  activate <addr> <port>    - Activate output port");
    Serial.println("  deactivate <addr> <port>  - Deactivate output port");
    Serial.println("  analog <addr> <pin>       - Read analog pin");
    Serial.println("  digital <addr> <pin>      - Read digital pin");
    Serial.println("  all_analog <addr>         - Read all analog pins");
    Serial.println("  all_digital <addr>        - Read all digital pins");
    Serial.println("  status <addr>             - Show sensor data for device");
    Serial.println();
  } else {
    Serial.println("CAN initialization failed");
  }
}

void doSerialControl()
{
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    // Parse command
    int space1 = command.indexOf(' ');
    int space2 = command.lastIndexOf(' ');
    
    if (space1 > 0) {
      String cmd = command.substring(0, space1);
      String addr_str = command.substring(space1 + 1, space2 > space1 ? space2 : command.length());
      String param_str = space2 > space1 ? command.substring(space2 + 1) : "";
      
      uint16_t device_addr = strtol(addr_str.c_str(), NULL, 16);
      uint8_t param = param_str.length() > 0 ? param_str.toInt() : 0;
      
      // Execute commands
      if (cmd == "activate") {
        if (param_str.length() > 0) {
          Serial.printf("Sending activate command to 0x%03X, port %d\n", device_addr, param);
          sendOutputCommand(device_addr, ACTIVATE_CMD, param);
        } else {
          Serial.println("Usage: activate <addr> <port>");
        }
      }
      else if (cmd == "deactivate") {
        if (param_str.length() > 0) {
          Serial.printf("Sending deactivate command to 0x%03X, port %d\n", device_addr, param);
          sendOutputCommand(device_addr, DEACTIVATE_CMD, param);
        } else {
          Serial.println("Usage: deactivate <addr> <port>");
        }
      }
      else if (cmd == "analog") {
        if (param_str.length() > 0) {
          Serial.printf("Requesting analog reading from 0x%03X, pin %d\n", device_addr, param);
          requestAnalogReading(device_addr, param);
        } else {
          Serial.println("Usage: analog <addr> <pin>");
        }
      }
      else if (cmd == "digital") {
        if (param_str.length() > 0) {
          Serial.printf("Requesting digital reading from 0x%03X, pin %d\n", device_addr, param);
          requestDigitalReading(device_addr, param);
        } else {
          Serial.println("Usage: digital <addr> <pin>");
        }
      }
      else if (cmd == "all_analog") {
        Serial.printf("Requesting all analog readings from 0x%03X\n", device_addr);
        requestAllAnalogReadings(device_addr);
      }
      else if (cmd == "all_digital") {
        Serial.printf("Requesting all digital readings from 0x%03X\n", device_addr);
        requestAllDigitalReadings(device_addr);
      }
      else if (cmd == "status") {
        printSensorData(device_addr);
      }
      else {
        Serial.println("Unknown command");
      }
    }
  }
}

void loop() {
  // Handle serial commands
  doSerialControl();

  // Process incoming CAN messages
  receiveCANMessages();

  handleSwitchPress();
  delay(10);
}

bool initializeCAN() {
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    Serial.println("Failed to install TWAI driver");
    return false;
  }
  
  if (twai_start() != ESP_OK) {
    Serial.println("Failed to start TWAI driver");
    return false;
  }
  
  return true;
}

void initializeSensorStorage() {
  for (int dev = 0; dev < MAX_DEVICES; dev++) {
    for (int pin = 0; pin < MAX_PINS; pin++) {
      analog_readings[dev][pin].valid = false;
      analog_readings[dev][pin].analog_value = 0;
      analog_readings[dev][pin].timestamp = 0;
      
      digital_readings[dev][pin].valid = false;
      digital_readings[dev][pin].digital_value = false;
      digital_readings[dev][pin].timestamp = 0;
    }
  }
}

bool sendOutputCommand(uint16_t device_address, uint8_t command, uint8_t port) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = command;
  message.data[1] = port;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool requestAnalogReading(uint16_t device_address, uint8_t pin) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = READ_ANALOG_CMD;
  message.data[1] = pin;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool requestDigitalReading(uint16_t device_address, uint8_t pin) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = READ_DIGITAL_CMD;
  message.data[1] = pin;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool requestAllAnalogReadings(uint16_t device_address) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 1;
  message.data[0] = READ_ALL_ANALOG_CMD;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool requestAllDigitalReadings(uint16_t device_address) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 1;
  message.data[0] = READ_ALL_DIGITAL_CMD;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

void receiveCANMessages() {
  twai_message_t message;
  
  if (twai_receive(&message, 0) == ESP_OK) {
    Serial.printf("Received from 0x%03X: ", message.identifier);
    for (int i = 0; i < message.data_length_code; i++) {
      Serial.printf("0x%02X ", message.data[i]);
    }
    Serial.println();
    
    processReceivedMessage(&message);
  }
}

void processReceivedMessage(twai_message_t* message) {
  if (message->data_length_code < 1) return;
  
  uint8_t response_type = message->data[0];
  
  switch (response_type) {
    case ACK_ACTIVATE:
    case ACK_DEACTIVATE:
      if (message->data_length_code >= 3) {
        Serial.printf("Output command acknowledged: Port %d, Status 0x%02X\n", 
                      message->data[1], message->data[2]);
      }
      break;
      
    case ANALOG_DATA:
      if (message->data_length_code >= 5) {
        uint8_t pin = message->data[1];
        uint16_t value = (message->data[2] << 8) | message->data[3];
        Serial.printf("Analog pin %d: %d (%.2fV)\n", pin, value, value * 3.3 / 4095.0);
        storeSensorReading(message->identifier, pin, value, true);
      }
      break;
      
    case DIGITAL_DATA:
      if (message->data_length_code >= 3) {
        uint8_t pin = message->data[1];
        bool value = message->data[2] != 0;
        Serial.printf("Digital pin %d: %s\n", pin, value ? "HIGH" : "LOW");
        storeSensorReading(message->identifier, pin, value ? 1 : 0, false);
      }
      break;
      
    case ALL_ANALOG_DATA:
      // Process multiple analog readings in one message
      for (int i = 1; i < message->data_length_code; i += 3) {
        if (i + 2 < message->data_length_code) {
          uint8_t pin = message->data[i];
          uint16_t value = (message->data[i+1] << 8) | message->data[i+2];
          Serial.printf("Analog pin %d: %d (%.2fV)\n", pin, value, value * 3.3 / 4095.0);
          storeSensorReading(message->identifier, pin, value, true);
        }
      }
      break;
      
    case ALL_DIGITAL_DATA:
      // Process multiple digital readings packed in bytes
      if (message->data_length_code >= 2) {
        uint8_t digital_data = message->data[1];
        for (int pin = 0; pin < 8; pin++) {
          bool value = (digital_data >> pin) & 1;
          Serial.printf("Digital pin %d: %s\n", pin, value ? "HIGH" : "LOW");
          storeSensorReading(message->identifier, pin, value ? 1 : 0, false);
        }
      }
      break;
      
    case ERROR_RESPONSE:
      if (message->data_length_code >= 3) {
        Serial.printf("Error response: Port/Pin %d, Error code 0x%02X\n", 
                      message->data[1], message->data[2]);
      }
      break;
  }
}

void storeSensorReading(uint16_t device_address, uint8_t pin, uint16_t value, bool is_analog) {
  uint8_t dev_index = getDeviceIndex(device_address);
  if (dev_index >= MAX_DEVICES || pin >= MAX_PINS) return;
  
  if (is_analog) {
    analog_readings[dev_index][pin].analog_value = value;
    analog_readings[dev_index][pin].valid = true;
    analog_readings[dev_index][pin].timestamp = millis();
  } else {
    digital_readings[dev_index][pin].digital_value = (value != 0);
    digital_readings[dev_index][pin].valid = true;
    digital_readings[dev_index][pin].timestamp = millis();
  }
}

SensorReading getAnalogReading(uint16_t device_address, uint8_t pin) {
  uint8_t dev_index = getDeviceIndex(device_address);
  if (dev_index < MAX_DEVICES && pin < MAX_PINS) {
    return analog_readings[dev_index][pin];
  }
  SensorReading invalid = {0, false, false, 0};
  return invalid;
}

SensorReading getDigitalReading(uint16_t device_address, uint8_t pin) {
  uint8_t dev_index = getDeviceIndex(device_address);
  if (dev_index < MAX_DEVICES && pin < MAX_PINS) {
    return digital_readings[dev_index][pin];
  }
  SensorReading invalid = {0, false, false, 0};
  return invalid;
}

void printSensorData(uint16_t device_address) {
  uint8_t dev_index = getDeviceIndex(device_address);
  if (dev_index >= MAX_DEVICES) {
    Serial.println("Invalid device address");
    return;
  }
  
  Serial.printf("\n=== Sensor Data for Device 0x%03X ===\n", device_address);
  
  Serial.println("Analog Pins:");
  bool found_analog = false;
  for (int pin = 0; pin < MAX_PINS; pin++) {
    if (analog_readings[dev_index][pin].valid) {
      unsigned long age = millis() - analog_readings[dev_index][pin].timestamp;
      Serial.printf("  Pin %d: %d (%.2fV) - %lu ms ago\n", 
                    pin, 
                    analog_readings[dev_index][pin].analog_value,
                    analog_readings[dev_index][pin].analog_value * 3.3 / 4095.0,
                    age);
      found_analog = true;
    }
  }
  if (!found_analog) Serial.println("  No analog data");
  
  Serial.println("Digital Pins:");
  bool found_digital = false;
  for (int pin = 0; pin < MAX_PINS; pin++) {
    if (digital_readings[dev_index][pin].valid) {
      unsigned long age = millis() - digital_readings[dev_index][pin].timestamp;
      Serial.printf("  Pin %d: %s - %lu ms ago\n", 
                    pin, 
                    digital_readings[dev_index][pin].digital_value ? "HIGH" : "LOW",
                    age);
      found_digital = true;
    }
  }
  if (!found_digital) Serial.println("  No digital data");
  
  Serial.println("================================\n");
}

uint8_t getDeviceIndex(uint16_t device_address) {
  // Simple mapping - you might want a more sophisticated approach
  return device_address % MAX_DEVICES;
}