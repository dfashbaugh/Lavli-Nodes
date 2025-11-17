#include "can_comm.h"

// Sensor data storage arrays
SensorReading analog_readings[MAX_DEVICES][MAX_PINS];
SensorReading digital_readings[MAX_DEVICES][MAX_PINS];

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

// bool sendMotorCommand(uint8_t command, uint8_t data) {
//   return sendCANMessage(MOTOR_CONTROL_ADDRESS, command, data);
// }

// bool sendWashCommand() {
//   Serial.println("[CAN] Sending WASH command to motor controller");
//   // Send motor start command with wash parameters
//   // For wash cycle: moderate RPM, forward direction
//   bool success = true;
//   success &= sendMotorCommand(MOTOR_SET_DIRECTION_CMD, 0x01); // Forward
//   success &= sendMotorCommand(MOTOR_SET_RPM_CMD, 150);        // 150 RPM for wash
//   return success;
// }

// bool sendDryCommand() {
//   Serial.println("[CAN] Sending DRY command to motor controller");
//   // Send motor start command with dry parameters  
//   // For dry cycle: higher RPM, forward direction
//   bool success = true;
//   success &= sendMotorCommand(MOTOR_SET_DIRECTION_CMD, 0x01); // Forward
//   success &= sendMotorCommand(MOTOR_SET_RPM_CMD, 250);        // 250 RPM for dry
//   return success;
// }

// bool sendStopCommand() {
//   Serial.println("[CAN] Sending STOP command to motor controller");
//   return sendMotorCommand(MOTOR_STOP_CMD, 0x00);
// }

// NOTE: Old processCANMessages() has been replaced with:
//       - receiveCANMessages() - receives messages and calls processor
//       - processReceivedMessage() - full switch-case processing with sensor storage
// These new functions are defined at the end of this file.

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

// ========================================
// Sensor Storage Management Functions
// ========================================

void initializeSensorStorage() {
  for (uint8_t dev = 0; dev < MAX_DEVICES; dev++) {
    for (uint8_t pin = 0; pin < MAX_PINS; pin++) {
      analog_readings[dev][pin].analog_value = 0;
      analog_readings[dev][pin].valid = false;
      analog_readings[dev][pin].timestamp = 0;

      digital_readings[dev][pin].digital_value = false;
      digital_readings[dev][pin].valid = false;
      digital_readings[dev][pin].timestamp = 0;
    }
  }
  Serial.println("[CAN] Sensor storage initialized");
}

uint8_t getDeviceIndex(uint16_t device_address) {
  return device_address % MAX_DEVICES;
}

void storeSensorReading(uint16_t device_address, uint8_t pin, uint16_t value, bool is_analog) {
  uint8_t dev_index = getDeviceIndex(device_address);

  if (pin >= MAX_PINS) {
    Serial.printf("[CAN] Warning: Pin %d out of range (max %d)\n", pin, MAX_PINS - 1);
    return;
  }

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
  SensorReading invalid_reading = {0, false, false, 0};

  if (pin >= MAX_PINS) {
    return invalid_reading;
  }

  uint8_t dev_index = getDeviceIndex(device_address);
  return analog_readings[dev_index][pin];
}

SensorReading getDigitalReading(uint16_t device_address, uint8_t pin) {
  SensorReading invalid_reading = {0, false, false, 0};

  if (pin >= MAX_PINS) {
    return invalid_reading;
  }

  uint8_t dev_index = getDeviceIndex(device_address);
  return digital_readings[dev_index][pin];
}

void printSensorData(uint16_t device_address) {
  uint8_t dev_index = getDeviceIndex(device_address);
  unsigned long current_time = millis();

  Serial.printf("\n========== Sensor Data for Device 0x%03X ==========\n", device_address);

  // Print analog readings
  Serial.println("\nAnalog Readings:");
  bool has_analog = false;
  for (uint8_t pin = 0; pin < MAX_PINS; pin++) {
    if (analog_readings[dev_index][pin].valid) {
      has_analog = true;
      uint16_t value = analog_readings[dev_index][pin].analog_value;
      float voltage = (value / 4095.0) * 3.3;  // Convert to voltage (assuming 12-bit ADC, 3.3V reference)
      unsigned long age = current_time - analog_readings[dev_index][pin].timestamp;
      Serial.printf("  Pin %d: %d (%.2fV) - Age: %lu ms\n", pin, value, voltage, age);
    }
  }
  if (!has_analog) {
    Serial.println("  No valid analog readings");
  }

  // Print digital readings
  Serial.println("\nDigital Readings:");
  bool has_digital = false;
  for (uint8_t pin = 0; pin < MAX_PINS; pin++) {
    if (digital_readings[dev_index][pin].valid) {
      has_digital = true;
      bool value = digital_readings[dev_index][pin].digital_value;
      unsigned long age = current_time - digital_readings[dev_index][pin].timestamp;
      Serial.printf("  Pin %d: %s - Age: %lu ms\n", pin, value ? "HIGH" : "LOW", age);
    }
  }
  if (!has_digital) {
    Serial.println("  No valid digital readings");
  }

  Serial.println("==================================================\n");
}

// ========================================
// Message Processing Functions
// ========================================

void processReceivedMessage(twai_message_t* message) {
  uint8_t command = message->data[0];

  switch (command) {
    case ACK_ACTIVATE: {
      uint8_t port = message->data[1];
      uint8_t status = message->data[2];
      Serial.printf("[CAN] ACK: Activate Port %d - Status: 0x%02X\n", port, status);
      break;
    }

    case ACK_DEACTIVATE: {
      uint8_t port = message->data[1];
      uint8_t status = message->data[2];
      Serial.printf("[CAN] ACK: Deactivate Port %d - Status: 0x%02X\n", port, status);
      break;
    }

    case ACK_MOTOR_RPM: {
      uint16_t confirmed_speed = (message->data[1] << 8) | message->data[2];
      Serial.printf("[CAN] ACK: Motor RPM set to %d\n", confirmed_speed);
      break;
    }

    case ACK_MOTOR_DIRECTION: {
      bool clockwise = (message->data[1] != 0);
      Serial.printf("[CAN] ACK: Motor direction set to %s\n", clockwise ? "CW" : "CCW");
      break;
    }

    case ACK_MOTOR_STOP: {
      Serial.println("[CAN] ACK: Motor stopped");
      break;
    }

    case ANALOG_DATA: {
      uint8_t pin = message->data[1];
      uint16_t value = (message->data[2] << 8) | message->data[3];
      Serial.printf("[CAN] Analog Data: Pin %d = %d\n", pin, value);
      storeSensorReading(message->identifier, pin, value, true);
      break;
    }

    case DIGITAL_DATA: {
      uint8_t pin = message->data[1];
      bool value = (message->data[2] != 0);
      Serial.printf("[CAN] Digital Data: Pin %d = %s\n", pin, value ? "HIGH" : "LOW");
      storeSensorReading(message->identifier, pin, value ? 1 : 0, false);
      break;
    }

    case ALL_ANALOG_DATA: {
      Serial.printf("[CAN] All Analog Data from 0x%03X:\n", message->identifier);
      // Data format: [command][pin0][high0][low0][pin1][high1][low1]...
      // Each reading takes 3 bytes
      uint8_t num_readings = (message->data_length_code - 1) / 3;
      for (uint8_t i = 0; i < num_readings && (1 + i * 3 + 2) < message->data_length_code; i++) {
        uint8_t pin = message->data[1 + i * 3];
        uint16_t value = (message->data[1 + i * 3 + 1] << 8) | message->data[1 + i * 3 + 2];
        Serial.printf("  Pin %d: %d\n", pin, value);
        storeSensorReading(message->identifier, pin, value, true);
      }
      break;
    }

    case ALL_DIGITAL_DATA: {
      Serial.printf("[CAN] All Digital Data from 0x%03X:\n", message->identifier);
      // Data format: [command][bitfield]
      // Each bit represents one pin (bit 0 = pin 0, bit 1 = pin 1, etc.)
      uint8_t bitfield = message->data[1];
      for (uint8_t pin = 0; pin < 8; pin++) {
        bool value = (bitfield & (1 << pin)) != 0;
        Serial.printf("  Pin %d: %s\n", pin, value ? "HIGH" : "LOW");
        storeSensorReading(message->identifier, pin, value ? 1 : 0, false);
      }
      break;
    }

    case MOTOR_STATUS_DATA: {
      uint8_t status = message->data[1];
      uint16_t current_rpm = (message->data[2] << 8) | message->data[3];
      bool direction = (message->data[4] != 0);
      Serial.printf("[CAN] Motor Status: Status=0x%02X, RPM=%d, Dir=%s\n",
                    status, current_rpm, direction ? "CW" : "CCW");
      break;
    }

    case ERROR_RESPONSE: {
      uint8_t port_or_pin = message->data[1];
      uint8_t error_code = message->data[2];
      Serial.printf("[CAN] ERROR: Port/Pin %d - Error Code: 0x%02X\n", port_or_pin, error_code);
      break;
    }

    default: {
      Serial.printf("[CAN] Unknown command: 0x%02X\n", command);
      break;
    }
  }
}

void receiveCANMessages() {
  twai_message_t message;

  // Check for received messages (non-blocking)
  while (twai_receive(&message, 0) == ESP_OK) {
    Serial.printf("[CAN] <<< Received from 0x%03X: ", message.identifier);
    for (uint8_t i = 0; i < message.data_length_code; i++) {
      Serial.printf("0x%02X ", message.data[i]);
    }
    Serial.println();

    // Process the message
    processReceivedMessage(&message);
  }
}