#include "can_comm.h"

// CAN Configuration structures
static twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
static twai_timing_config_t t_config = CAN_BAUD_RATE;
static twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

bool initializeCAN() {
  Serial.println("[CAN] Initializing CAN communication...");
  Serial.printf("[CAN] TX Pin: GPIO%d, RX Pin: GPIO%d\n", CAN_TX_PIN, CAN_RX_PIN);
  
  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    Serial.println("[CAN] Failed to install TWAI driver");
    return false;
  }
  
  // Start TWAI driver
  if (twai_start() != ESP_OK) {
    Serial.println("[CAN] Failed to start TWAI driver");
    return false;
  }
  
  Serial.println("[CAN] CAN communication initialized successfully");
  return true;
}

bool sendCANMessage(uint16_t address, uint8_t command, uint8_t data) {
  twai_message_t message;
  
  message.identifier = address;
  message.extd = 0;  // Standard frame format
  message.rtr = 0;   // Data frame
  message.data_length_code = 2;
  message.data[0] = command;
  message.data[1] = data;
  
  // Clear unused data bytes
  for (int i = 2; i < 8; i++) {
    message.data[i] = 0;
  }
  
  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    Serial.printf("[CAN] Message sent - Address: 0x%03X, Command: 0x%02X, Data: 0x%02X\n", 
                  address, command, data);
    return true;
  } else {
    Serial.printf("[CAN] Failed to send message - Error: %s\n", esp_err_to_name(result));
    return false;
  }
}

bool sendMotorCommand(uint8_t command, uint8_t data) {
  return sendCANMessage(MOTOR_CONTROL_ADDRESS, command, data);
}

bool sendWashCommand() {
  Serial.println("[CAN] Sending WASH command to motor controller");
  // Send motor start command with wash parameters
  // For wash cycle: moderate RPM, forward direction
  bool success = true;
  success &= sendMotorCommand(MOTOR_SET_DIRECTION_CMD, 0x01); // Forward
  success &= sendMotorCommand(MOTOR_SET_RPM_CMD, 150);        // 150 RPM for wash
  return success;
}

bool sendDryCommand() {
  Serial.println("[CAN] Sending DRY command to motor controller");
  // Send motor start command with dry parameters  
  // For dry cycle: higher RPM, forward direction
  bool success = true;
  success &= sendMotorCommand(MOTOR_SET_DIRECTION_CMD, 0x01); // Forward
  success &= sendMotorCommand(MOTOR_SET_RPM_CMD, 250);        // 250 RPM for dry
  return success;
}

bool sendStopCommand() {
  Serial.println("[CAN] Sending STOP command to motor controller");
  return sendMotorCommand(MOTOR_STOP_CMD, 0x00);
}

void processCANMessages() {
  twai_message_t message;
  
  // Check for received messages (non-blocking)
  if (twai_receive(&message, 0) == ESP_OK) {
    Serial.printf("[CAN] Received message - Address: 0x%03X, Command: 0x%02X, Data: 0x%02X\n",
                  message.identifier, message.data[0], message.data[1]);
    
    // Process motor responses
    if (message.identifier == MOTOR_CONTROL_ADDRESS) {
      switch (message.data[0]) {
        case MOTOR_ACK_RPM:
          Serial.printf("[CAN] Motor ACK: RPM set to %d\n", message.data[1]);
          break;
        case MOTOR_ACK_DIRECTION:
          Serial.printf("[CAN] Motor ACK: Direction set to %s\n", 
                       message.data[1] ? "Forward" : "Reverse");
          break;
        case MOTOR_ACK_STOP:
          Serial.println("[CAN] Motor ACK: Motor stopped");
          break;
        case MOTOR_ACK_STATUS:
          Serial.printf("[CAN] Motor Status: %d\n", message.data[1]);
          break;
        case MOTOR_ERROR_RESPONSE:
          Serial.println("[CAN] Motor Error Response");
          break;
        default:
          Serial.printf("[CAN] Unknown motor response: 0x%02X\n", message.data[0]);
          break;
      }
    }
  }
}