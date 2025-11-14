#include "can_comm.h"

// CAN Configuration structures
static twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
static twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();//CAN_BAUD_RATE;
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
        case ACK_MOTOR_RPM:
          Serial.printf("[CAN] Motor ACK: RPM set to %d\n", message.data[1]);
          break;
        case ACK_MOTOR_DIRECTION:
          Serial.printf("[CAN] Motor ACK: Direction set to %s\n",
                       message.data[1] ? "Forward" : "Reverse");
          break;
        case ACK_MOTOR_STOP:
          Serial.println("[CAN] Motor ACK: Motor stopped");
          break;
        case MOTOR_STATUS_DATA:
          Serial.printf("[CAN] Motor Status: %d\n", message.data[1]);
          break;
        case ERROR_RESPONSE:
          Serial.println("[CAN] Motor Error Response");
          break;
        default:
          Serial.printf("[CAN] Unknown motor response: 0x%02X\n", message.data[0]);
          break;
      }
    }
  }
}

// Pin control functions (from master node)

bool sendOutputCommand(uint16_t device_address, uint8_t command, uint8_t port) {
  twai_message_t message;

  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = command;
  message.data[1] = port;

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    Serial.printf("[CAN] Output command sent - Address: 0x%03X, Command: 0x%02X, Port: %d\n",
                  device_address, command, port);
    return true;
  }
  return false;
}

bool requestAnalogReading(uint16_t device_address, uint8_t pin) {
  twai_message_t message;

  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = READ_ANALOG_CMD;
  message.data[1] = pin;

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    Serial.printf("[CAN] Analog read request sent - Address: 0x%03X, Pin: %d\n",
                  device_address, pin);
    return true;
  }
  return false;
}

bool requestDigitalReading(uint16_t device_address, uint8_t pin) {
  twai_message_t message;

  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = READ_DIGITAL_CMD;
  message.data[1] = pin;

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    Serial.printf("[CAN] Digital read request sent - Address: 0x%03X, Pin: %d\n",
                  device_address, pin);
    return true;
  }
  return false;
}

bool requestAllAnalogReadings(uint16_t device_address) {
  twai_message_t message;

  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 1;
  message.data[0] = READ_ALL_ANALOG_CMD;

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    Serial.printf("[CAN] All analog read request sent - Address: 0x%03X\n", device_address);
    return true;
  }
  return false;
}

bool requestAllDigitalReadings(uint16_t device_address) {
  twai_message_t message;

  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 1;
  message.data[0] = READ_ALL_DIGITAL_CMD;

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    Serial.printf("[CAN] All digital read request sent - Address: 0x%03X\n", device_address);
    return true;
  }
  return false;
}

bool sendMotorRPM(uint16_t device_address, uint16_t rpm) {
  twai_message_t message;

  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 3;
  message.data[0] = MOTOR_SET_RPM_CMD;
  message.data[1] = (rpm >> 8) & 0xFF;  // High byte
  message.data[2] = rpm & 0xFF;         // Low byte

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    Serial.printf("[CAN] Motor RPM command sent - Address: 0x%03X, RPM: %d\n",
                  device_address, rpm);
    return true;
  }
  return false;
}

bool sendMotorDirection(uint16_t device_address, bool clockwise) {
  twai_message_t message;

  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = MOTOR_SET_DIRECTION_CMD;
  message.data[1] = clockwise ? 1 : 0;

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    Serial.printf("[CAN] Motor direction command sent - Address: 0x%03X, Direction: %s\n",
                  device_address, clockwise ? "CW" : "CCW");
    return true;
  }
  return false;
}

bool sendMotorStop(uint16_t device_address) {
  twai_message_t message;

  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 1;
  message.data[0] = MOTOR_STOP_CMD;

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    Serial.printf("[CAN] Motor stop command sent - Address: 0x%03X\n", device_address);
    return true;
  }
  return false;
}

bool requestMotorStatus(uint16_t device_address) {
  twai_message_t message;

  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 1;
  message.data[0] = MOTOR_STATUS_CMD;

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));
  if (result == ESP_OK) {
    Serial.printf("[CAN] Motor status request sent - Address: 0x%03X\n", device_address);
    return true;
  }
  return false;
}

bool sendGenericCANMessage(uint16_t address, uint8_t* data, uint8_t data_length) {
  if (data_length > 8) {
    Serial.println("[CAN] Data length exceeds maximum (8 bytes)");
    return false;
  }

  twai_message_t message;

  message.identifier = address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = data_length;

  for (uint8_t i = 0; i < data_length; i++) {
    message.data[i] = data[i];
  }

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));

  if (result == ESP_OK) {
    Serial.printf("[CAN] Generic message sent to 0x%03X: ", address);
    for (uint8_t i = 0; i < data_length; i++) {
      Serial.printf("0x%02X ", data[i]);
    }
    Serial.println();
    return true;
  } else {
    Serial.printf("[CAN] Failed to send message to 0x%03X\n", address);
    return false;
  }
}