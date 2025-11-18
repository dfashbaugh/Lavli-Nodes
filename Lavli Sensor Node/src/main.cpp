#include <Arduino.h>
#include <driver/twai.h>

// CAN pins - using valid ESP32-S3 GPIO pins
#define CAN_TX_PIN GPIO_NUM_4
#define CAN_RX_PIN GPIO_NUM_5

#define LEFT_BOARD  // Comment this line for right board configuration

#ifdef LEFT_BOARD
  // Left board specific pin definitions can go here
// Device CAN address - Change this for each sensor device on your network
#define MY_CAN_ADDRESS 0x124

// Analog pin definitions - Map analog pin numbers to GPIO pins
#define ANALOG_PIN_0 GPIO_NUM_8   // Temp Back
#define ANALOG_PIN_1 GPIO_NUM_7   // Temp Front
#define ANALOG_PIN_2 GPIO_NUM_10 // flow sensor (TODO: We will have special handling for this)

#define FLOW_SENSOR_PIN ANALOG_PIN_2

// Digital pin definitions - Map digital pin numbers to GPIO pins
#define DIGITAL_PIN_0 GPIO_NUM_1 // Clean Tank low
#define DIGITAL_PIN_1 GPIO_NUM_2 // Clean Tank Mid
#define DIGITAL_PIN_2 GPIO_NUM_3 // Clean Tank High

// Maximum number of pins supported
#define MAX_ANALOG_PINS 3
#define MAX_DIGITAL_PINS 3

// Pin mapping arrays
const int analog_pins[MAX_ANALOG_PINS] = {
  ANALOG_PIN_0, ANALOG_PIN_1, ANALOG_PIN_2
};

const int digital_pins[MAX_DIGITAL_PINS] = {
  DIGITAL_PIN_0, DIGITAL_PIN_1, DIGITAL_PIN_2
};

volatile uint32_t pulseStart = 0;
volatile uint32_t pulseWidth = 0;
volatile bool newPulse = false;
uint32_t pulseCount = 0;
uint32_t pulsesPerSecond = 0;

unsigned long pulseCheckTime = 0;

void IRAM_ATTR pulseISR() {
    if (digitalRead(FLOW_SENSOR_PIN) == HIGH) {
        pulseStart = micros();
    } else {
        pulseWidth = micros() - pulseStart;
        newPulse = true;
        pulseCount++;
    }
}

#else 

#define MY_CAN_ADDRESS 0x128

// Analog pin definitions - Map analog pin numbers to GPIO pins
#define ANALOG_PIN_0 GPIO_NUM_11   // Pressure Sensor 


// Digital pin definitions - Map digital pin numbers to GPIO pins
#define DIGITAL_PIN_0 GPIO_NUM_1 // Dirty Tank Low
#define DIGITAL_PIN_1 GPIO_NUM_2 // Dirty Tank Mid
#define DIGITAL_PIN_2 GPIO_NUM_3 // Dirty Tank High
#define DIGITAL_PIN_3 GPIO_NUM_6 // Leak Detection Front
#define DIGITAL_PIN_4 GPIO_NUM_8 // Leak Detection Back
#define DIGITAL_PIN_5 GPIO_NUM_9 // Purge Low
#define DIGITAL_PIN_6 GPIO_NUM_10 // Purge High
// Maximum number of pins supported
#define MAX_ANALOG_PINS 1
#define MAX_DIGITAL_PINS 7

// Pin mapping arrays
const int analog_pins[MAX_ANALOG_PINS] = {
  ANALOG_PIN_0
};

const int digital_pins[MAX_DIGITAL_PINS] = {
  DIGITAL_PIN_0, DIGITAL_PIN_1, DIGITAL_PIN_2, DIGITAL_PIN_3,
  DIGITAL_PIN_4, DIGITAL_PIN_5, DIGITAL_PIN_6
};

#endif

// Command definitions
#define READ_ANALOG_CMD     0x03
#define READ_DIGITAL_CMD    0x04
#define READ_ALL_ANALOG_CMD 0x05
#define READ_ALL_DIGITAL_CMD 0x06

// Response command definitions
#define ANALOG_DATA         0x20
#define DIGITAL_DATA        0x21
#define ALL_ANALOG_DATA     0x22
#define ALL_DIGITAL_DATA    0x23
#define ERROR_RESPONSE      0xFF

// Function prototypes
bool initializeCAN();
void initializePins();
uint16_t readAnalogPin(int pin_number);
bool readDigitalPin(int pin_number);
void receiveCANMessages();
void processReceivedMessage(twai_message_t* message);
bool sendAnalogData(uint8_t pin, uint16_t value);
bool sendDigitalData(uint8_t pin, bool value);
bool sendAllAnalogData();
bool sendAllDigitalData();
bool sendErrorResponse(uint8_t pin, uint8_t error_code);
int getAnalogGPIOForPin(int pin_number);
int getDigitalGPIOForPin(int pin_number);

// CAN configuration
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

void setup() {
  Serial.begin(115200);
  Serial.printf("CAN Sensor Device - Address: 0x%03X\n", MY_CAN_ADDRESS);
  
  delay(2000);
  
  // Initialize pins
  initializePins();
  
  // Initialize CAN
  if (initializeCAN()) {
    Serial.println("CAN initialized successfully");
    Serial.println("Ready to respond to sensor requests...");
    Serial.println("\nSupported commands:");
    Serial.println("  0x03 - Read single analog pin");
    Serial.println("  0x04 - Read single digital pin");  
    Serial.println("  0x05 - Read all analog pins");
    Serial.println("  0x06 - Read all digital pins");
  } else {
    Serial.println("CAN initialization failed");
  }
  
  // Test readings on startup
  Serial.println("\nInitial sensor readings:");
  for (int i = 0; i < MAX_ANALOG_PINS; i++) {
    uint16_t value = readAnalogPin(i);
    Serial.printf("Analog pin %d: %d (%.2fV)\n", i, value, value * 3.3 / 4095.0);
  }
  
  for (int i = 0; i < MAX_DIGITAL_PINS; i++) {
    bool value = readDigitalPin(i);
    Serial.printf("Digital pin %d: %s\n", i, value ? "HIGH" : "LOW");
  }
  
  Serial.println();

#ifdef FLOW_SENSOR_PIN
  // Attach interrupt for flow sensor pulse measurement
  pinMode(FLOW_SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseISR, CHANGE);
  Serial.println("Flow sensor interrupt attached");
#endif
}

void loop() {
  // Continuously listen for CAN messages
  receiveCANMessages();
  delay(10); // Small delay to prevent overwhelming the CPU

#ifdef FLOW_SENSOR_PIN
  if(millis() - pulseCheckTime >= 1000) {
    pulseCheckTime = millis();
    pulsesPerSecond = pulseCount;
    pulseCount = 0;
    Serial.printf("Pulses in last second: %d\n", pulsesPerSecond);
  }
#endif
}

bool initializeCAN() {
  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    Serial.println("Failed to install TWAI driver");
    return false;
  }
  
  // Start TWAI driver
  if (twai_start() != ESP_OK) {
    Serial.println("Failed to start TWAI driver");
    return false;
  }
  
  Serial.println("TWAI driver installed and started");
  return true;
}

void initializePins() {
  Serial.println("Initializing sensor pins...");
  
  // Initialize analog pins (ESP32 analog pins don't need explicit pinMode)
  for (int pin = 0; pin < MAX_ANALOG_PINS; pin++) {
    int gpio_pin = getAnalogGPIOForPin(pin);
    if (gpio_pin != -1) {
      // Analog pins are ready by default
      Serial.printf("Analog pin %d -> GPIO %d ready\n", pin, gpio_pin);
    }
  }
  
  // Initialize digital input pins
  for (int pin = 0; pin < MAX_DIGITAL_PINS; pin++) {
    int gpio_pin = getDigitalGPIOForPin(pin);
    if (gpio_pin != -1) {
      pinMode(gpio_pin, INPUT_PULLUP); // Use pull-up resistors
      Serial.printf("Digital pin %d -> GPIO %d initialized as INPUT_PULLUP\n", pin, gpio_pin);
    }
  }
  
  Serial.println("Pin initialization complete");
}

int getAnalogGPIOForPin(int pin_number) {
  if (pin_number >= 0 && pin_number < MAX_ANALOG_PINS) {
    return analog_pins[pin_number];
  }
  return -1; // Invalid pin
}

int getDigitalGPIOForPin(int pin_number) {
  if (pin_number >= 0 && pin_number < MAX_DIGITAL_PINS) {
    return digital_pins[pin_number];
  }
  return -1; // Invalid pin
}

uint16_t readAnalogPin(int pin_number) {
  int gpio_pin = getAnalogGPIOForPin(pin_number);
  
  if (gpio_pin == -1) {
    Serial.printf("ERROR: Invalid analog pin number %d\n", pin_number);
    return 0;
  }
  
  #ifdef FLOW_SENSOR_PIN
  if (gpio_pin == FLOW_SENSOR_PIN) {
    // Special handling for flow sensor can go here
    // For now, just read normally
  }
  #endif

  uint16_t reading = analogRead(gpio_pin);
  return reading;
}

bool readDigitalPin(int pin_number) {
  int gpio_pin = getDigitalGPIOForPin(pin_number);
  
  if (gpio_pin == -1) {
    Serial.printf("ERROR: Invalid digital pin number %d\n", pin_number);
    return false;
  }
  
  return digitalRead(gpio_pin) == HIGH;
}

void receiveCANMessages() {
  twai_message_t message;
  
  // Check for incoming messages (non-blocking)
  if (twai_receive(&message, 0) == ESP_OK) {
    // Only process messages addressed to this device
    if (message.identifier == MY_CAN_ADDRESS) {
      Serial.printf("Received message for my address (0x%03X): ", message.identifier);
      
      for (int i = 0; i < message.data_length_code; i++) {
        Serial.printf("0x%02X ", message.data[i]);
      }
      Serial.println();
      
      // Process the received message
      processReceivedMessage(&message);
    }
    // Optionally log messages for other devices (for debugging)
    else {
      Serial.printf("Message for other device (0x%03X) - ignoring\n", message.identifier);
    }
  }
}

void processReceivedMessage(twai_message_t* message) {
  if (message->data_length_code < 1) {
    Serial.println("ERROR: Message too short");
    sendErrorResponse(0, 0x01); // Error: Invalid message length
    return;
  }
  
  uint8_t command = message->data[0];
  
  Serial.printf("Processing command 0x%02X\n", command);
  
  switch (command) {
    case READ_ANALOG_CMD:
      if (message->data_length_code >= 2) {
        uint8_t pin = message->data[1];
        Serial.printf("Reading analog pin %d\n", pin);
        
        if (pin < MAX_ANALOG_PINS) {
          uint16_t value = readAnalogPin(pin);
          sendAnalogData(pin, value);
        } else {
          sendErrorResponse(pin, 0x04); // Error: Invalid pin number
        }
      } else {
        sendErrorResponse(0, 0x01); // Error: Invalid message length
      }
      break;
      
    case READ_DIGITAL_CMD:
      if (message->data_length_code >= 2) {
        uint8_t pin = message->data[1];
        Serial.printf("Reading digital pin %d\n", pin);
        
        if (pin < MAX_DIGITAL_PINS) {
          bool value = readDigitalPin(pin);
          sendDigitalData(pin, value);
        } else {
          sendErrorResponse(pin, 0x04); // Error: Invalid pin number
        }
      } else {
        sendErrorResponse(0, 0x01); // Error: Invalid message length
      }
      break;
      
    case READ_ALL_ANALOG_CMD:
      Serial.println("Reading all analog pins");
      sendAllAnalogData();
      break;
      
    case READ_ALL_DIGITAL_CMD:
      Serial.println("Reading all digital pins");
      sendAllDigitalData();
      break;
      
    default:
      Serial.printf("ERROR: Unknown command 0x%02X\n", command);
      sendErrorResponse(0, 0x03); // Error: Unknown command
      break;
  }
}

bool sendAnalogData(uint8_t pin, uint16_t value) {
  twai_message_t response;
  
  response.identifier = MY_CAN_ADDRESS;
  response.extd = 0;
  response.rtr = 0;
  response.data_length_code = 4;
  
  response.data[0] = ANALOG_DATA;
  response.data[1] = pin;
  response.data[2] = (value >> 8) & 0xFF; // High byte
  response.data[3] = value & 0xFF;        // Low byte
  
  if (twai_transmit(&response, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.printf("Sent analog data: Pin=%d, Value=%d\n", pin, value);
    return true;
  } else {
    Serial.println("Failed to send analog data");
    return false;
  }
}

bool sendDigitalData(uint8_t pin, bool value) {
  twai_message_t response;
  
  response.identifier = MY_CAN_ADDRESS;
  response.extd = 0;
  response.rtr = 0;
  response.data_length_code = 3;
  
  response.data[0] = DIGITAL_DATA;
  response.data[1] = pin;
  response.data[2] = value ? 1 : 0;
  
  if (twai_transmit(&response, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.printf("Sent digital data: Pin=%d, Value=%s\n", pin, value ? "HIGH" : "LOW");
    return true;
  } else {
    Serial.println("Failed to send digital data");
    return false;
  }
}

bool sendAllAnalogData() {
  // Send analog data in multiple messages if needed
  // Each message can contain up to 2 analog readings (3 bytes each: pin + 2-byte value)
  
  for (int start_pin = 0; start_pin < MAX_ANALOG_PINS; start_pin += 2) {
    twai_message_t response;
    response.identifier = MY_CAN_ADDRESS;
    response.extd = 0;
    response.rtr = 0;
    response.data[0] = ALL_ANALOG_DATA;
    
    int data_index = 1;
    int pins_in_message = 0;
    
    // Pack up to 2 analog readings per message
    for (int pin = start_pin; pin < start_pin + 2 && pin < MAX_ANALOG_PINS; pin++) {
      uint16_t value = readAnalogPin(pin);
      
      response.data[data_index++] = pin;                    // Pin number
      response.data[data_index++] = (value >> 8) & 0xFF;   // High byte
      response.data[data_index++] = value & 0xFF;          // Low byte
      pins_in_message++;
    }
    
    response.data_length_code = data_index;
    
    if (twai_transmit(&response, pdMS_TO_TICKS(1000)) == ESP_OK) {
      Serial.printf("Sent batch of %d analog readings starting from pin %d\n", pins_in_message, start_pin);
    } else {
      Serial.printf("Failed to send analog data batch starting from pin %d\n", start_pin);
      return false;
    }
    
    delay(5); // Small delay between messages
  }
  
  return true;
}

bool sendAllDigitalData() {
  twai_message_t response;
  
  response.identifier = MY_CAN_ADDRESS;
  response.extd = 0;
  response.rtr = 0;
  response.data_length_code = 2;
  
  response.data[0] = ALL_DIGITAL_DATA;
  
  // Pack all digital readings into a single byte (8 bits for 8 pins)
  uint8_t digital_data = 0;
  for (int pin = 0; pin < MAX_DIGITAL_PINS; pin++) {
    if (readDigitalPin(pin)) {
      digital_data |= (1 << pin);
    }
  }
  
  response.data[1] = digital_data;
  
  if (twai_transmit(&response, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.printf("Sent all digital data: 0b");
    for (int i = 7; i >= 0; i--) {
      Serial.printf("%d", (digital_data >> i) & 1);
    }
    Serial.println();
    return true;
  } else {
    Serial.println("Failed to send all digital data");
    return false;
  }
}

bool sendErrorResponse(uint8_t pin, uint8_t error_code) {
  twai_message_t response;
  
  response.identifier = MY_CAN_ADDRESS;
  response.extd = 0;
  response.rtr = 0;
  response.data_length_code = 3;
  
  response.data[0] = ERROR_RESPONSE;
  response.data[1] = pin;
  response.data[2] = error_code;
  
  if (twai_transmit(&response, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.printf("Sent error response: Pin=%d, Error=0x%02X\n", pin, error_code);
    return true;
  } else {
    Serial.println("Failed to send error response");
    return false;
  }
}