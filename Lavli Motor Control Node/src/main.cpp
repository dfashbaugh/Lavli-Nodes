#include <Arduino.h>
#include <driver/twai.h>
// Removed SoftwareSerial as ESP32 uses HardwareSerial

// Device CAN address - This should match CONTROLLER_MOTOR_ADDRESS in master (0x311)
#define MY_CAN_ADDRESS 0x311

// CAN pins - using valid ESP32-S3 GPIO pins (matching master node)
#define CAN_TX_PIN GPIO_NUM_4
#define CAN_RX_PIN GPIO_NUM_5

// Motor controller serial pins
#define MOTOR_TX_PIN 18  // TX to motor controller (ESP32 TX -> Motor RX)
#define MOTOR_PULSE_PIN 8
#define MOTOR_RX_PIN 17  // RX from motor controller (ESP32 RX -> Motor TX)

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

// Use HardwareSerial for motor communication
HardwareSerial motorSerial(1); // Use UART1

// Motor state variables
uint16_t commandedRPM = 0;
uint16_t currentRPM = 0;
uint16_t actualRPM = 0;
String currentDirection = "CCW";  // Default direction
byte faultCode = 0;
bool motorRunning = false;

// Serial communication
String motorResponse = "";
unsigned long lastStatusRequest = 0;
const unsigned long statusInterval = 1000; // Request status every second

// CAN configuration
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// Function prototypes
bool initializeCAN();
void receiveCANMessages();
void processReceivedMessage(twai_message_t* message);
bool sendResponse(uint8_t command, uint16_t data1, uint16_t data2, uint8_t status);
void setMotorRPM(uint16_t rpm);
void setMotorDirection(bool clockwise);
void stopMotor();
void requestMotorStatus();
void readMotorResponse();
void parseMotorStatus(String response);

void setup() {
  Serial.begin(115200);
  Serial.printf("CAN Motor Controller - Address: 0x%03X\n", MY_CAN_ADDRESS);

  delay(2000);

  // Initialize motor serial communication
  pinMode(MOTOR_PULSE_PIN, OUTPUT);
  digitalWrite(MOTOR_PULSE_PIN, LOW); // Ensure pulse pin is low
  motorSerial.begin(9600, SERIAL_8N1, MOTOR_RX_PIN, MOTOR_TX_PIN); // Initialize UART1 with pins
  Serial.println("Motor SoftwareSerial initialized at 9600 baud");

  // Initialize CAN
  if (initializeCAN()) {
    Serial.println("CAN initialized successfully");
    Serial.println("Ready to receive motor commands...");
  } else {
    Serial.println("CAN initialization failed");
  }

  // Send initial status request to motor
  delay(1000);
  requestMotorStatus();
}

void sendRPMAsPulse()
{
  pinMode(MOTOR_PULSE_PIN, INPUT);
  delay(commandedRPM/4);
  pinMode(MOTOR_PULSE_PIN, OUTPUT);
  digitalWrite(MOTOR_PULSE_PIN, LOW);
  delay(commandedRPM/4);
}

void loop() {
  // commandedRPM = 50;

  // Listen for CAN messages
  receiveCANMessages();
  
  // Read responses from motor controller
  readMotorResponse();
  
  // Periodically request motor status
  if (millis() - lastStatusRequest > statusInterval) {
    requestMotorStatus();
    lastStatusRequest = millis();
    Serial.println("Requested motor status update");
  }

  // sendRPMAsPulse();

  delay(10);
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
    sendResponse(ERROR_RESPONSE, 0, 0, 0x01); // Error: Invalid message length
    return;
  }
  
  uint8_t command = message->data[0];
  
  Serial.printf("Processing motor command 0x%02X\n", command);
  
  switch (command) {
    case MOTOR_SET_RPM_CMD:
      if (message->data_length_code >= 3) {
        uint16_t rpm = (message->data[1] << 8) | message->data[2];
        Serial.printf("Setting motor RPM to: %d\n", rpm);
        setMotorRPM(rpm);
        sendResponse(ACK_MOTOR_RPM, rpm, 0, 0x00); // Success
      } else {
        Serial.println("ERROR: RPM command too short");
        sendResponse(ERROR_RESPONSE, 0, 0, 0x02); // Error: Invalid parameters
      }
      break;
      
    case MOTOR_SET_DIRECTION_CMD:
      if (message->data_length_code >= 2) {
        bool clockwise = message->data[1] != 0;
        Serial.printf("Setting motor direction to: %s\n", clockwise ? "CW" : "CCW");
        setMotorDirection(clockwise);
        sendResponse(ACK_MOTOR_DIRECTION, clockwise ? 1 : 0, 0, 0x00); // Success
      } else {
        Serial.println("ERROR: Direction command too short");
        sendResponse(ERROR_RESPONSE, 0, 0, 0x02); // Error: Invalid parameters
      }
      break;
      
    case MOTOR_STOP_CMD:
      Serial.println("Stopping motor");
      stopMotor();
      sendResponse(ACK_MOTOR_STOP, 0, 0, 0x00); // Success
      break;
      
    case MOTOR_STATUS_CMD:
      Serial.println("Motor status requested");
      requestMotorStatus();
      // Response will be sent when motor replies
      break;
      
    default:
      Serial.printf("ERROR: Unknown motor command 0x%02X\n", command);
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
    Serial.printf("Response sent: Command=0x%02X, Data1=%d, Data2=%d, Status=0x%02X\n", 
                  command, data1, data2, status);
    return true;
  } else {
    Serial.printf("Failed to send response\n");
    return false;
  }
}

void setMotorRPM(uint16_t rpm) {
  // Limit RPM to safe range
  if (rpm > 1500) rpm = 1500;
  
  currentRPM = rpm;
  motorRunning = (rpm > 0);
  
  // Send SETRPM command to motor controller via SoftwareSerial
  motorSerial.print("SETRPM:");
  motorSerial.println(rpm);
  
  Serial.printf("Sent to motor: SETRPM:%d\n", rpm);
}

void setMotorDirection(bool clockwise) {
  currentDirection = clockwise ? "CW" : "CCW";
  
  // Send direction command to motor controller via SoftwareSerial
  if (clockwise) {
    motorSerial.println("SETCW");
    Serial.println("Sent to motor: SETCW");
  } else {
    motorSerial.println("SETCCW");
    Serial.println("Sent to motor: SETCCW");
  }
}

void stopMotor() {
  currentRPM = 0;
  motorRunning = false;
  
  // Send stop command (set RPM to 0) via SoftwareSerial
  motorSerial.println("SETRPM:0");
  Serial.println("Sent to motor: SETRPM:0");
}

void requestMotorStatus() {
  // Request status from motor controller via SoftwareSerial
  motorSerial.println("STATUS");
}

void readMotorResponse() {
  while (motorSerial.available()) {
    // Serial.println("Reading from motor serial...");

    char c = motorSerial.read();
    
    if (c == '\n' || c == '\r') {
      if (motorResponse.length() > 0) {
        Serial.print("Motor response: ");
        Serial.println(motorResponse);
        
        // Parse the response
        if (motorResponse.startsWith("STATUS:")) {
          parseMotorStatus(motorResponse);
        } else if (motorResponse == "OK") {
          Serial.println("Command acknowledged by motor controller");
        } else if (motorResponse == "ERROR") {
          Serial.println("Motor controller reported command error");
        }
        
        motorResponse = "";
      }
    } else {
      motorResponse += c;
    }
  }
}

void parseMotorStatus(String response) {
  // Expected format: STATUS:DIRECTION,RPM,ACTUAL_RPM,FAULT
  // Example: STATUS:CW,1200,1198,0
  
  int colonPos = response.indexOf(':');
  if (colonPos == -1) return;
  
  String data = response.substring(colonPos + 1);
  
  // Parse comma-separated values
  int comma1 = data.indexOf(',');
  int comma2 = data.indexOf(',', comma1 + 1);
  int comma3 = data.indexOf(',', comma2 + 1);
  
  if (comma1 != -1 && comma2 != -1 && comma3 != -1) {
    currentDirection = data.substring(0, comma1);
    currentRPM = data.substring(comma1 + 1, comma2).toInt();
    actualRPM = data.substring(comma2 + 1, comma3).toInt();
    faultCode = data.substring(comma3 + 1).toInt();
    
    Serial.printf("Motor Status - Dir: %s, Set RPM: %d, Actual RPM: %d, Fault: 0x%02X\n",
                  currentDirection.c_str(), currentRPM, actualRPM, faultCode);
    
    // Send status data via CAN
    uint16_t statusData1 = actualRPM;
    uint16_t statusData2 = (currentDirection == "CW" ? 0x8000 : 0x0000) | (currentRPM & 0x7FFF);
    sendResponse(MOTOR_STATUS_DATA, statusData1, statusData2, faultCode);
  }
}