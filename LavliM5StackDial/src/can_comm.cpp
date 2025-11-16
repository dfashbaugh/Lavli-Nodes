#include "can_comm.h"

// CAN Configuration structures
// NO_ACK mode: For testing without a CAN transceiver (loopback only)
// NORMAL mode: For production with CAN transceiver connected
// #define CAN_TEST_MODE TWAI_MODE_NO_ACK  // Change to TWAI_MODE_NORMAL when CAN transceiver connected
#define CAN_TEST_MODE TWAI_MODE_NORMAL  // Change to TWAI_MODE_NORMAL when CAN transceiver connected


// Configure with alerts enabled for debugging
// Automatic bus recovery enabled to handle BUS_OFF errors
static twai_general_config_t g_config = {
  .mode = CAN_TEST_MODE,
  .tx_io = CAN_TX_PIN,
  .rx_io = CAN_RX_PIN,  // Keep RX pin enabled
  .clkout_io = TWAI_IO_UNUSED,
  .bus_off_io = TWAI_IO_UNUSED,
  .tx_queue_len = 5,
  .rx_queue_len = 5,
  .alerts_enabled = TWAI_ALERT_TX_SUCCESS | TWAI_ALERT_TX_FAILED | TWAI_ALERT_ERR_PASS |
                    TWAI_ALERT_BUS_ERROR | TWAI_ALERT_ARB_LOST | TWAI_ALERT_BUS_OFF,
  .clkout_divider = 0,
  .intr_flags = ESP_INTR_FLAG_LEVEL1
};

static twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
static twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

bool initializeCAN() {
  Serial.println("[CAN] ========================================");
  Serial.println("[CAN] Initializing CAN/TWAI communication...");
  Serial.printf("[CAN] TX Pin: GPIO%d, RX Pin: GPIO%d\n", CAN_TX_PIN, CAN_RX_PIN);
  Serial.printf("[CAN] Mode: %s\n", (CAN_TEST_MODE == TWAI_MODE_NO_ACK) ? "NO_ACK (Test)" : "NORMAL");
  Serial.printf("[CAN] Baud Rate: 500 kbps\n");

  // Install TWAI driver
  Serial.println("[CAN] Installing TWAI driver...");
  Serial.flush();
  delay(10);

  esp_err_t err = twai_driver_install(&g_config, &t_config, &f_config);

  Serial.printf("[CAN] twai_driver_install returned: %s (0x%X)\n", esp_err_to_name(err), err);
  Serial.flush();

  if (err != ESP_OK) {
    Serial.printf("[CAN] FAILED to install TWAI driver! Error: %s\n", esp_err_to_name(err));
    Serial.flush();
    return false;
  }
  Serial.println("[CAN] TWAI driver installed successfully");
  Serial.flush();
  delay(10);

  // Start TWAI driver
  Serial.println("[CAN] Starting TWAI driver...");
  Serial.flush();
  delay(10);

  err = twai_start();

  Serial.printf("[CAN] twai_start returned: %s (0x%X)\n", esp_err_to_name(err), err);
  Serial.flush();

  if (err != ESP_OK) {
    Serial.printf("[CAN] FAILED to start TWAI driver! Error: %s\n", esp_err_to_name(err));
    Serial.flush();
    return false;
  }
  Serial.println("[CAN] TWAI driver started successfully");
  Serial.flush();
  delay(10);

  // Check initial status
  twai_status_info_t status;
  if (twai_get_status_info(&status) == ESP_OK) {
    Serial.println("[CAN] Initial TWAI Status:");
    Serial.printf("[CAN]   State: %s\n",
                  (status.state == TWAI_STATE_RUNNING) ? "RUNNING" :
                  (status.state == TWAI_STATE_BUS_OFF) ? "BUS_OFF" :
                  (status.state == TWAI_STATE_RECOVERING) ? "RECOVERING" : "STOPPED");
    Serial.printf("[CAN]   TX Error Count: %d\n", status.tx_error_counter);
    Serial.printf("[CAN]   RX Error Count: %d\n", status.rx_error_counter);
  }

  Serial.println("[CAN] ========================================");
  Serial.println("[CAN] CAN communication initialized successfully!");
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

  Serial.printf("[CAN] >>> Attempting to send: Address=0x%03X, Cmd=0x%02X, Data=0x%02X\n",
                address, command, data);

  // Check alerts before sending
  uint32_t alerts;
  if (twai_read_alerts(&alerts, 0) == ESP_OK && alerts != 0) {
    Serial.printf("[CAN] >>> Alerts before TX: 0x%08X\n", alerts);
  }

  esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(1000));

  // Check alerts after sending
  if (twai_read_alerts(&alerts, pdMS_TO_TICKS(100)) == ESP_OK) {
    if (alerts & TWAI_ALERT_TX_SUCCESS) {
      Serial.println("[CAN] >>> ALERT: TX_SUCCESS");
    }
    if (alerts & TWAI_ALERT_TX_FAILED) {
      Serial.println("[CAN] >>> ALERT: TX_FAILED");
    }
    if (alerts & TWAI_ALERT_ERR_PASS) {
      Serial.println("[CAN] >>> ALERT: ERR_PASS (error passive state)");
    }
    if (alerts & TWAI_ALERT_BUS_ERROR) {
      Serial.println("[CAN] >>> ALERT: BUS_ERROR");
    }
    if (alerts & TWAI_ALERT_ARB_LOST) {
      Serial.println("[CAN] >>> ALERT: ARB_LOST (arbitration lost)");
    }
  }

  if (result == ESP_OK) {
    Serial.printf("[CAN] >>> SUCCESS! Message queued for transmission\n");
    return true;
  } else {
    Serial.printf("[CAN] >>> FAILED! Error: %s\n", esp_err_to_name(result));

    // Check bus status on failure
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK) {
      Serial.printf("[CAN] >>> Bus State: %s, TX_ERR: %d, RX_ERR: %d\n",
                    (status.state == TWAI_STATE_RUNNING) ? "RUNNING" :
                    (status.state == TWAI_STATE_BUS_OFF) ? "BUS_OFF" :
                    (status.state == TWAI_STATE_RECOVERING) ? "RECOVERING" : "STOPPED",
                    status.tx_error_counter, status.rx_error_counter);

      // Automatic bus recovery if BUS_OFF
      if (status.state == TWAI_STATE_BUS_OFF) {
        Serial.println("[CAN] >>> Initiating bus recovery...");
        if (twai_initiate_recovery() == ESP_OK) {
          Serial.println("[CAN] >>> Bus recovery initiated successfully");
          delay(50); // Give time for recovery

          // Try the message again after recovery
          Serial.println("[CAN] >>> Retrying message after recovery...");
          result = twai_transmit(&message, pdMS_TO_TICKS(1000));
          if (result == ESP_OK) {
            Serial.println("[CAN] >>> Retry SUCCESS!");
            return true;
          } else {
            Serial.printf("[CAN] >>> Retry FAILED: %s\n", esp_err_to_name(result));
          }
        } else {
          Serial.println("[CAN] >>> Bus recovery failed!");
        }
      }
    }
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