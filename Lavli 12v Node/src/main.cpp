#include <Arduino.h>
#include <driver/twai.h>

// #define DC_12V_BOARD
#define AC_120V_BOARD

// CAN pins - using valid ESP32-S3 GPIO pins
#define CAN_TX_PIN GPIO_NUM_4
#define CAN_RX_PIN GPIO_NUM_5

// Device CAN address - Change this for each device on your network
#define MY_CAN_ADDRESS 0x543

// Port pin definitions - Map port numbers to GPIO pins
#ifdef DC_12V_BOARD
#define MAX_PORTS 3
#define PORT_1_PIN GPIO_NUM_14
#define PORT_2_PIN GPIO_NUM_15
#define PORT_3_PIN GPIO_NUM_16

// Port pin mapping array
const int port_pins[MAX_PORTS + 1] = {
  -1,           // Port 0 (invalid)
  PORT_1_PIN,   // Port 1
  PORT_2_PIN,   // Port 2
  PORT_3_PIN,   // Port 3
};
#endif

#ifdef AC_120V_BOARD
#define MAX_PORTS 2
#define PORT_1_PIN GPIO_NUM_18
#define PORT_2_PIN GPIO_NUM_17

// Port pin mapping array
const int port_pins[MAX_PORTS + 1] = {
  -1,           // Port 0 (invalid)
  PORT_1_PIN,   // Port 1
  PORT_2_PIN,   // Port 2
};
#endif
// #define PORT_4_PIN GPIO_NUM_6
// #define PORT_5_PIN GPIO_NUM_7
// #define PORT_6_PIN GPIO_NUM_18
// #define PORT_7_PIN GPIO_NUM_19
// #define PORT_8_PIN GPIO_NUM_17

// Maximum number of ports supported


// Message command definitions
#define ACTIVATE_CMD   0x01
#define DEACTIVATE_CMD 0x02

// Response command definitions
#define ACK_ACTIVATE   0x10
#define ACK_DEACTIVATE 0x11
#define ERROR_RESPONSE 0xFF

// Function prototypes
bool initializeCAN();
void initializePorts();
bool activatePort(int port_number);
bool deactivatePort(int port_number);
void receiveCANMessages();
void processReceivedMessage(twai_message_t* message);
bool sendResponse(uint8_t command, uint8_t port, uint8_t status);
int getGPIOForPort(int port_number);



// Port status tracking
bool port_status[MAX_PORTS + 1] = {false}; // All ports start deactivated

// CAN configuration
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();  // 500kbps
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

void setup() {
  Serial.begin(115200);
  Serial.printf("CAN Receiver Device - Address: 0x%03X\n", MY_CAN_ADDRESS);

  delay(5000);

  for(int i = 0; i < 10; i++){
    Serial.println("Test");
    delay(100);
  }
  
  // // Initialize port pins
  initializePorts();
  
  // // Initialize CAN
  if (initializeCAN()) {
    Serial.println("CAN initialized successfully");
    Serial.println("Ready to receive commands...");
  } else {
    Serial.println("CAN initialization failed");
  }
}

void loop() {
  // Serial.println("Looping...");
  // delay(1000);

  // Continuously listen for CAN messages
  receiveCANMessages();
  delay(10); // Small delay to prevent overwhelming the CPU

  // digitalWrite(PORT_1_PIN, HIGH);
  // digitalWrite(PORT_2_PIN, HIGH);
  // Serial.println("Write High");
  // delay(2000);
  // digitalWrite(PORT_1_PIN, LOW);
  // digitalWrite(PORT_2_PIN, LOW);
  // Serial.println("Write Low");
  // delay(2000);


  // digitalWrite(GPIO_NUM_14, HIGH);
  // digitalWrite(GPIO_NUM_15, HIGH);
  // digitalWrite(GPIO_NUM_16, HIGH);
  // Serial.println("Write High");
  // delay(1000);
  // digitalWrite(GPIO_NUM_14, LOW);
  // digitalWrite(GPIO_NUM_15, LOW);
  // digitalWrite(GPIO_NUM_16, LOW);
  // Serial.println("Write Low");
  // delay(1000);
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

void initializePorts() {
  Serial.println("Initializing port pins...");
  
  for (int port = 1; port <= MAX_PORTS; port++) {
    int gpio_pin = getGPIOForPort(port);
    if (gpio_pin != -1) {
      pinMode(gpio_pin, OUTPUT);
      
      digitalWrite(gpio_pin, LOW); // Start with all ports OFF
      port_status[port] = false;
      Serial.printf("Port %d -> GPIO %d initialized (OFF)\n", port, gpio_pin);
    }
  }
  
  Serial.println("Port initialization complete");
}

int getGPIOForPort(int port_number) {
  if (port_number >= 1 && port_number <= MAX_PORTS) {
    return port_pins[port_number];
  }
  return -1; // Invalid port
}

bool activatePort(int port_number) {
  Serial.println("ACTIVATE PORT");

  int gpio_pin = getGPIOForPort(port_number);
  
  if (gpio_pin == -1) {
    Serial.printf("ERROR: Invalid port number %d\n", port_number);
    return false;
  }
  
  digitalWrite(gpio_pin, HIGH);
  // Serial.println(gpio_pin + " set to HIGH");
  port_status[port_number] = true;
  Serial.printf("Port %d (GPIO %d) ACTIVATED\n", port_number, gpio_pin);
  return true;
}

bool deactivatePort(int port_number) {
  Serial.println("DEACTIVATE PORT");

  int gpio_pin = getGPIOForPort(port_number);
  
  if (gpio_pin == -1) {
    Serial.printf("ERROR: Invalid port number %d\n", port_number);
    return false;
  }
  
  digitalWrite(gpio_pin, LOW);
  // Serial.println(gpio_pin + " set to LOW");
  port_status[port_number] = false;
  Serial.printf("Port %d (GPIO %d) DEACTIVATED\n", port_number, gpio_pin);
  return true;
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
  if (message->data_length_code < 2) {
    Serial.println("ERROR: Message too short");
    sendResponse(ERROR_RESPONSE, 0, 0x01); // Error: Invalid message length
    return;
  }
  
  uint8_t command = message->data[0];
  uint8_t port = message->data[1];
  
  Serial.printf("Processing command 0x%02X for port %d\n", command, port);
  
  switch (command) {
    case ACTIVATE_CMD:
      if (activatePort(port)) {
        sendResponse(ACK_ACTIVATE, port, 0x00); // Success
      } else {
        sendResponse(ERROR_RESPONSE, port, 0x02); // Error: Invalid port
      }
      break;
      
    case DEACTIVATE_CMD:
      if (deactivatePort(port)) {
        sendResponse(ACK_DEACTIVATE, port, 0x00); // Success
      } else {
        sendResponse(ERROR_RESPONSE, port, 0x02); // Error: Invalid port
      }
      break;
      
    default:
      Serial.printf("ERROR: Unknown command 0x%02X\n", command);
      sendResponse(ERROR_RESPONSE, port, 0x03); // Error: Unknown command
      break;
  }
}

bool sendResponse(uint8_t command, uint8_t port, uint8_t status) {
  twai_message_t response;
  
  // Configure response message
  response.identifier = MY_CAN_ADDRESS; // Response from this device's address
  response.extd = 0;           // Standard frame (11-bit ID)
  response.rtr = 0;            // Data frame, not remote request
  response.data_length_code = 3; // 3 bytes of data
  
  // Response data: [Response Command, Port Number, Status]
  response.data[0] = command;
  response.data[1] = port;
  response.data[2] = status; // 0x00 = success, non-zero = error code
  
  // Send response
  if (twai_transmit(&response, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.printf("Response sent: Command=0x%02X, Port=%d, Status=0x%02X\n", 
                  command, port, status);
    return true;
  } else {
    Serial.printf("Failed to send response\n");
    return false;
  }
}