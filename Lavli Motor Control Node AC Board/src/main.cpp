#include <Arduino.h>
#include <driver/twai.h>

// Device CAN address - This should match CONTROLLER_MOTOR_ADDRESS in master (0x311)
#define MY_CAN_ADDRESS 0x311

#define USE_CAN

// CAN pins - using valid ESP32-S3 GPIO pins (matching master node)
#define CAN_TX_PIN GPIO_NUM_4
#define CAN_RX_PIN GPIO_NUM_5

#define INVERTER_SERIAL Serial1  // Leonardo: Serial1 = TX1 (pin 1), RX1 (pin 0)
#define DEBUG_SERIAL Serial      // For debugging over USB
#define BAUD_RATE 2400

// Direct UART pins to inverter (replacing the intermediate motor controller)
// #define INVERTER_TX_PIN 18  // TX to inverter (ESP32 TX -> Inverter COM)
// #define INVERTER_RX_PIN 17  // RX from inverter (ESP32 RX -> Inverter DATA)

// Motor control command definitions (from master)
#define MOTOR_SET_RPM_CMD     0x30
#define MOTOR_SET_DIRECTION_CMD 0x31
#define MOTOR_STOP_CMD        0x32
#define MOTOR_STATUS_CMD      0x33

// Response command definitions (to master)
#define ACK_MOTOR_RPM         0x40
#define ACK_MOTOR_DIRECTION   0x41
#define ACK_MOTOR_STOP        0x42
#define MOTOR_STATUS_DATA     0x43
#define ERROR_RESPONSE        0xFF

// Inverter Command Definitions
#define CMD_CW    0x14
#define CMD_CCW   0x18
#define CMD_UNB   0x27
#define CMD_LOAD  0xB0
#define CMD_ACCSPD 0xA0

#define PORT_1_PIN GPIO_NUM_18
#define PORT_2_PIN GPIO_NUM_17

// Use HardwareSerial for inverter communication
// HardwareSerial inverterSerial(1); // Use UART1

// Motor state variables
uint16_t commandedRPM = 0;
uint16_t currentRPM = 0;
uint16_t targetRPM = 0;
uint16_t actualRPM = 0;
byte currentDirection = CMD_CCW;  // Default direction (CCW)
byte faultCode = 0;
bool motorRunning = false;

// Deceleration Profile
const uint16_t DECEL_STEP_SIZE = 30;     // RPM to decrease per step
const unsigned long DECEL_INTERVAL = 600; // Time between deceleration steps in milliseconds
unsigned long lastDecelTime = 0;         // Last time deceleration was applied
bool isDecelerating = false;             // Flag to indicate if we're currently decelerating

// Communication Timing
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 300;  // milliseconds
unsigned long lastStatusRequest = 0;
const unsigned long statusInterval = 1000; // Request status every second

// CAN configuration
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// Inverter Communication Buffers
byte txBuffer[10];
byte rxBuffer[10];

// CRC Lookup Table for inverter communication
const uint16_t aCrcTab[] = {
  0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
  0x8408, 0x9489, 0xa50a, 0xb58b, 0xc60c, 0xd68d, 0xe70e, 0xf78f
};

// Function prototypes
bool initializeCAN();
void receiveCANMessages();
void processReceivedMessage(twai_message_t* message);
bool sendResponse(uint8_t command, uint16_t data1, uint16_t data2, uint8_t status);
void setMotorRPM(uint16_t rpm);
void setMotorDirection(bool clockwise);
void stopMotor();
void requestMotorStatus();

// Inverter communication functions
uint16_t calcCRC16(byte* buffer, uint16_t length);
void sendInverterCommand(byte cmd, uint16_t rpm = 0, byte acc = 0);
bool readInverterResponse();
void handleDeceleration();

void setup() {
  DEBUG_SERIAL.begin(115200);
  DEBUG_SERIAL.printf("Integrated CAN Motor Controller - Address: 0x%03X\n", MY_CAN_ADDRESS);

  pinMode(PORT_1_PIN, OUTPUT);
  pinMode(PORT_2_PIN, OUTPUT);
  digitalWrite(PORT_1_PIN, HIGH);
  digitalWrite(PORT_2_PIN, HIGH);

  delay(2000);

  // Initialize inverter serial communication (direct to inverter)
  // inverterSerial.begin(2400, SERIAL_8N1, INVERTER_RX_PIN, INVERTER_TX_PIN); // 2400 baud for inverter
  // INVERTER_SERIAL.begin(BAUD_RATE); // Using default UART1 pins
  INVERTER_SERIAL.begin(BAUD_RATE, SERIAL_8N1, 44, 43);
  DEBUG_SERIAL.println("Inverter UART initialized at 2400 baud");

#ifdef USE_CAN
  // Initialize CAN
  if (initializeCAN()) {
    DEBUG_SERIAL.println("CAN initialized successfully");
    DEBUG_SERIAL.println("Ready to receive motor commands...");
  } else {
    DEBUG_SERIAL.println("CAN initialization failed");
  }
#endif

  delay(1000);
}

void loop() {
#ifdef USE_CAN
  // Listen for CAN messages
  receiveCANMessages();
#endif

  // Handle deceleration if active
  handleDeceleration();
  
  // Send periodic commands to maintain inverter state
  unsigned long now = millis();
  if (now - lastSendTime > sendInterval) {
    lastSendTime = now;
    sendInverterCommand(currentDirection, currentRPM);
  }
  
  // Read responses from inverter
  readInverterResponse();
  
  delay(10);
}

bool initializeCAN() {
  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    DEBUG_SERIAL.println("Failed to install TWAI driver");
    return false;
  }
  
  // Start TWAI driver
  if (twai_start() != ESP_OK) {
    DEBUG_SERIAL.println("Failed to start TWAI driver");
    return false;
  }
  
  DEBUG_SERIAL.println("TWAI driver installed and started");
  return true;
}

void receiveCANMessages() {
  twai_message_t message;
  
  // Check for incoming messages (non-blocking)
  if (twai_receive(&message, 0) == ESP_OK) {
    // Only process messages addressed to this device
    if (message.identifier == MY_CAN_ADDRESS) {
      DEBUG_SERIAL.printf("Received message for my address (0x%03X): ", message.identifier);
      
      for (int i = 0; i < message.data_length_code; i++) {
        DEBUG_SERIAL.printf("0x%02X ", message.data[i]);
      }
      DEBUG_SERIAL.println();
      
      // Process the received message
      processReceivedMessage(&message);
    }
    // Optionally log messages for other devices (for debugging)
    else {
      DEBUG_SERIAL.printf("Message for other device (0x%03X) - ignoring\n", message.identifier);
    }
  }
}

void processReceivedMessage(twai_message_t* message) {
  if (message->data_length_code < 1) {
    DEBUG_SERIAL.println("ERROR: Message too short");
    sendResponse(ERROR_RESPONSE, 0, 0, 0x01); // Error: Invalid message length
    return;
  }
  
  uint8_t command = message->data[0];
  
  DEBUG_SERIAL.printf("Processing motor command 0x%02X\n", command);
  
  switch (command) {
    case MOTOR_SET_RPM_CMD:
      if (message->data_length_code >= 3) {
        uint16_t rpm = (message->data[1] << 8) | message->data[2];
        DEBUG_SERIAL.printf("Setting motor RPM to: %d\n", rpm);
        setMotorRPM(rpm);
        sendResponse(ACK_MOTOR_RPM, rpm, 0, 0x00); // Success
      } else {
        DEBUG_SERIAL.println("ERROR: RPM command too short");
        sendResponse(ERROR_RESPONSE, 0, 0, 0x02); // Error: Invalid parameters
      }
      break;
      
    case MOTOR_SET_DIRECTION_CMD:
      if (message->data_length_code >= 2) {
        bool clockwise = message->data[1] != 0;
        DEBUG_SERIAL.printf("Setting motor direction to: %s\n", clockwise ? "CW" : "CCW");
        setMotorDirection(clockwise);
        sendResponse(ACK_MOTOR_DIRECTION, clockwise ? 1 : 0, 0, 0x00); // Success
      } else {
        DEBUG_SERIAL.println("ERROR: Direction command too short");
        sendResponse(ERROR_RESPONSE, 0, 0, 0x02); // Error: Invalid parameters
      }
      break;
      
    case MOTOR_STOP_CMD:
      DEBUG_SERIAL.println("Stopping motor");
      stopMotor();
      sendResponse(ACK_MOTOR_STOP, 0, 0, 0x00); // Success
      break;
      
    case MOTOR_STATUS_CMD:
      DEBUG_SERIAL.println("Motor status requested");
      requestMotorStatus();
      break;
      
    default:
      DEBUG_SERIAL.printf("ERROR: Unknown motor command 0x%02X\n", command);
      sendResponse(ERROR_RESPONSE, 0, 0, 0x03); // Error: Unknown command
      break;
  }
}

bool sendResponse(uint8_t command, uint16_t data1, uint16_t data2, uint8_t status) {
  twai_message_t response;
  
  // Configure response message
  response.identifier = MY_CAN_ADDRESS; // Response from this device's address
  response.extd = 0;           // Standard frame (11-bit ID)
  response.rtr = 0;            // Data frame, not remote request
  response.data_length_code = 6; // 6 bytes of data
  
  // Response data: [Response Command, Data1_H, Data1_L, Data2_H, Data2_L, Status]
  response.data[0] = command;
  response.data[1] = (data1 >> 8) & 0xFF;
  response.data[2] = data1 & 0xFF;
  response.data[3] = (data2 >> 8) & 0xFF;
  response.data[4] = data2 & 0xFF;
  response.data[5] = status; // 0x00 = success, non-zero = error code
  
  // Send response
  if (twai_transmit(&response, pdMS_TO_TICKS(1000)) == ESP_OK) {
    DEBUG_SERIAL.printf("Response sent: Command=0x%02X, Data1=%d, Data2=%d, Status=0x%02X\n", 
                  command, data1, data2, status);
    return true;
  } else {
    DEBUG_SERIAL.printf("Failed to send response\n");
    return false;
  }
}

void setMotorRPM(uint16_t rpm) {
  // Limit RPM to safe range
  if (rpm > 1500) rpm = 1500;
  
  commandedRPM = rpm;
  motorRunning = (rpm > 0);
  
  // Check if we need to decelerate
  if (rpm < currentRPM) {
    // Set up deceleration
    targetRPM = rpm;
    isDecelerating = true;
    lastDecelTime = millis(); // Start deceleration immediately
    
    DEBUG_SERIAL.printf("Starting deceleration from %d to %d\n", currentRPM, targetRPM);
  } else {
    // For acceleration or same speed, set immediately
    currentRPM = rpm;
    targetRPM = rpm;
    isDecelerating = false;
    
    DEBUG_SERIAL.printf("Setting RPM directly to: %d\n", currentRPM);
    
    // Send command with current direction and new RPM
    sendInverterCommand(currentDirection, currentRPM);
  }
}

void setMotorDirection(bool clockwise) {
  currentDirection = clockwise ? CMD_CW : CMD_CCW;
  
  DEBUG_SERIAL.printf("Setting direction to: %s\n", clockwise ? "CW" : "CCW");
  
  // Send direction command with current RPM to inverter
  sendInverterCommand(currentDirection, currentRPM);
}

void stopMotor() {
  commandedRPM = 0;
  currentRPM = 0;
  targetRPM = 0;
  motorRunning = false;
  isDecelerating = false;
  
  DEBUG_SERIAL.println("Stopping motor (RPM = 0)");
  
  // Send stop command (set RPM to 0) to inverter
  sendInverterCommand(currentDirection, 0);
}

void requestMotorStatus() {
  // Send current status data via CAN immediately
  // Status format: actualRPM in data1, direction and currentRPM in data2
  uint16_t statusData1 = actualRPM;
  uint16_t statusData2 = (currentDirection == CMD_CW ? 0x8000 : 0x0000) | (currentRPM & 0x7FFF);
  sendResponse(MOTOR_STATUS_DATA, statusData1, statusData2, faultCode);
  
  DEBUG_SERIAL.printf("Status sent - Actual RPM: %d, Set RPM: %d, Dir: %s, Fault: 0x%02X\n",
                actualRPM, currentRPM, 
                (currentDirection == CMD_CW) ? "CW" : "CCW", faultCode);
}

void handleDeceleration() {
  if (!isDecelerating) return;
  
  unsigned long now = millis();
  if (now - lastDecelTime > DECEL_INTERVAL) {
    lastDecelTime = now;
    
    // Decrease RPM by step size
    if (currentRPM > targetRPM + DECEL_STEP_SIZE) {
      currentRPM -= DECEL_STEP_SIZE;
      DEBUG_SERIAL.printf("Decelerating to: %d\n", currentRPM);
    } else {
      // We've reached or are very close to target
      currentRPM = targetRPM;
      isDecelerating = false;
      DEBUG_SERIAL.printf("Reached target RPM: %d\n", currentRPM);
    }
    
    // Send updated RPM command to inverter
    sendInverterCommand(currentDirection, currentRPM);
  }
}

// =====================
// Inverter Communication Functions
// =====================

uint16_t calcCRC16(byte* buffer, uint16_t length) {
  uint16_t wCrc = 0xffff;
  for (uint16_t i = 0; i < length; i++) {
    wCrc = (wCrc >> 4) ^ aCrcTab[(wCrc & 0x0f) ^ (buffer[i] & 0x0f)];
    wCrc = (wCrc >> 4) ^ aCrcTab[(wCrc & 0x0f) ^ (buffer[i] >> 4)];
  }
  return ~wCrc;
}

void sendInverterCommand(byte cmd, uint16_t rpm, byte acc) {
  memset(txBuffer, 0, sizeof(txBuffer));

  txBuffer[0] = cmd;

  // For CW/CCW: set speed in ωH/ωL
  // For ACCSPD: rpm = acceleration
  if (cmd == CMD_CW || cmd == CMD_CCW) {
    txBuffer[1] = (rpm >> 8) & 0xFF;  // ωH
    txBuffer[2] = rpm & 0xFF;         // ωL
  } else if (cmd == CMD_ACCSPD) {
    txBuffer[1] = acc;  // acceleration in RPM/s
  }

  // P3–P6 remain 0
  uint16_t crc = calcCRC16(txBuffer, 8);
  txBuffer[8] = (crc >> 8) & 0xFF;
  txBuffer[9] = crc & 0xFF;

  INVERTER_SERIAL.write(txBuffer, 10);

  DEBUG_SERIAL.printf(">> Sent CMD 0x%02X RPM: %d\n", cmd, rpm);
}

bool readInverterResponse() {
  if (INVERTER_SERIAL.available() >= 10) {
    INVERTER_SERIAL.readBytes(rxBuffer, 10);
    
    DEBUG_SERIAL.print("<< Inverter Response: ");
    for (int i = 0; i < 10; i++) {
      DEBUG_SERIAL.printf("0x%02X ", rxBuffer[i]);
    }
    DEBUG_SERIAL.println();

    uint16_t crcCalc = calcCRC16(rxBuffer, 8);
    uint16_t crcRecv = (rxBuffer[8] << 8) | rxBuffer[9];

    if (crcCalc != crcRecv) {
      DEBUG_SERIAL.println("!! CRC mismatch");
      return false;
    }

    // Extract actual RPM and fault code from response
    actualRPM = (rxBuffer[2] << 8) | rxBuffer[3];
    faultCode = rxBuffer[5];

    if (rxBuffer[0] == 0x06) {
      DEBUG_SERIAL.println("ACK received from inverter");
    } else if (rxBuffer[0] == 0x15) {
      DEBUG_SERIAL.println("NCK received from inverter");
    }

    DEBUG_SERIAL.printf("Actual RPM: %d, Fault Code: 0x%02X\n", actualRPM, faultCode);
    
    return true;
  }
  return false;
}