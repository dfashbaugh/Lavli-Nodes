#include <Arduino.h>
#include <driver/twai.h>

// CAN pins - using valid ESP32-S3 GPIO pins
#define CAN_TX_PIN GPIO_NUM_21
#define CAN_RX_PIN GPIO_NUM_20

// Message IDs for activate/deactivate commands
#define ACTIVATE_CMD   0x01
#define DEACTIVATE_CMD 0x02

// Function prototypes
bool initializeCAN();
bool activatePort(uint32_t can_address, int port_number);
bool deactivatePort(uint32_t can_address, int port_number);
void receiveCANMessages();
void processReceivedMessage(twai_message_t* message);
bool sendCANMessage(uint32_t can_address, uint8_t* data, uint8_t length);

// CAN configuration
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();  // 500kbps
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

void setup() {
  Serial.begin(115200);
  
  // Initialize CAN
  if (initializeCAN()) {
    Serial.println("CAN initialized successfully");
  } else {
    Serial.println("CAN initialization failed");
  }
}

void loop() {
  // Example usage
  delay(5000);
  
  // Activate port 3 on device with CAN address 0x123
  activatePort(0x123, 3);
  
  delay(2000);
  
  // Deactivate port 3 on device with CAN address 0x123
  deactivatePort(0x123, 3);
  
  // Listen for incoming messages (optional)
  receiveCANMessages();
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

bool activatePort(uint32_t can_address, int port_number) {
  twai_message_t message;
  
  // Configure message
  message.identifier = can_address;
  message.extd = 0;           // Standard frame (11-bit ID)
  message.rtr = 0;            // Data frame, not remote request
  message.data_length_code = 3; // 3 bytes of data
  
  // Message data: [Command, Port Number, Reserved]
  message.data[0] = ACTIVATE_CMD;
  message.data[1] = (uint8_t)port_number;
  message.data[2] = 0x00;     // Reserved byte
  
  // Send message
  if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.printf("ACTIVATE sent to 0x%03X, Port %d\n", can_address, port_number);
    return true;
  } else {
    Serial.printf("Failed to send ACTIVATE to 0x%03X, Port %d\n", can_address, port_number);
    return false;
  }
}

bool deactivatePort(uint32_t can_address, int port_number) {
  twai_message_t message;
  
  // Configure message
  message.identifier = can_address;
  message.extd = 0;           // Standard frame (11-bit ID)
  message.rtr = 0;            // Data frame, not remote request
  message.data_length_code = 3; // 3 bytes of data
  
  // Message data: [Command, Port Number, Reserved]
  message.data[0] = DEACTIVATE_CMD;
  message.data[1] = (uint8_t)port_number;
  message.data[2] = 0x00;     // Reserved byte
  
  // Send message
  if (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK) {
    Serial.printf("DEACTIVATE sent to 0x%03X, Port %d\n", can_address, port_number);
    return true;
  } else {
    Serial.printf("Failed to send DEACTIVATE to 0x%03X, Port %d\n", can_address, port_number);
    return false;
  }
}

// Optional: Function to receive and process incoming CAN messages
void receiveCANMessages() {
  twai_message_t message;
  
  // Check for incoming messages (non-blocking)
  if (twai_receive(&message, 0) == ESP_OK) {
    Serial.printf("Received message from 0x%03X: ", message.identifier);
    
    for (int i = 0; i < message.data_length_code; i++) {
      Serial.printf("0x%02X ", message.data[i]);
    }
    Serial.println();
    
    // Process the received message based on your protocol
    processReceivedMessage(&message);
  }
}

void processReceivedMessage(twai_message_t* message) {
  // Add your message processing logic here
  // For example, handle acknowledgments, status updates, etc.
  
  if (message->data_length_code >= 2) {
    uint8_t command = message->data[0];
    uint8_t port = message->data[1];
    
    switch (command) {
      case 0x10: // Example: ACK for activate
        Serial.printf("ACK: Port %d activated on device 0x%03X\n", port, message->identifier);
        break;
      case 0x11: // Example: ACK for deactivate
        Serial.printf("ACK: Port %d deactivated on device 0x%03X\n", port, message->identifier);
        break;
      case 0xFF: // Example: Error response
        Serial.printf("ERROR: Command failed for port %d on device 0x%03X\n", port, message->identifier);
        break;
      default:
        Serial.printf("Unknown command 0x%02X from device 0x%03X\n", command, message->identifier);
        break;
    }
  }
}

// Utility function to send custom CAN messages
bool sendCANMessage(uint32_t can_address, uint8_t* data, uint8_t length) {
  if (length > 8) {
    Serial.println("Error: CAN message length cannot exceed 8 bytes");
    return false;
  }
  
  twai_message_t message;
  message.identifier = can_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = length;
  
  for (int i = 0; i < length; i++) {
    message.data[i] = data[i];
  }
  
  return (twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK);
}