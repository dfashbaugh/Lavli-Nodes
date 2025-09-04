#include <Arduino.h>

// WIRING
// TX -> COM (COM is the command pin on the inverter)
// RX -> DATA (DATA is the receive data pin on the inverter)
// 5v -> VCC (VCC is the power pin on the inverter)
// GND -> GND (GND is the ground pin on the inverter)

#define BUILT_IN_LED 13  // Built-in LED pin for debugging
bool currentState = LOW; // Initial state of the LED

// =====================
// UART Settings
// =====================
#define INVERTER_SERIAL Serial1  // Leonardo: Serial1 = TX1 (pin 1), RX1 (pin 0)
#define DEBUG_SERIAL Serial      // For debugging over USB
#define BAUD_RATE 2400

// =====================
// Communication Timing
// =====================
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 300;  // milliseconds
unsigned long lastStatusTime = 0;
const unsigned long statusInterval = 500; // Status update interval in milliseconds

// =====================
// Command Definitions
// =====================
#define CMD_CW    0x14
#define CMD_CCW   0x18
#define CMD_UNB   0x27
#define CMD_LOAD  0xB0
#define CMD_ACCSPD 0xA0

// =====================
// Motor State Variables
// =====================
uint16_t currentRPM = 0;       // Current RPM setting
uint16_t targetRPM = 0;        // Target RPM to reach (for deceleration)
byte currentDirection = CMD_CCW; // Default direction is counter-clockwise
String inputBuffer = "";       // Buffer for incoming commands
uint16_t actualRPM = 0;        // Actual RPM reported by the inverter

// =====================
// Deceleration Profile
// =====================
const uint16_t DECEL_STEP_SIZE = 30;     // RPM to decrease per step
const unsigned long DECEL_INTERVAL = 600; // Time between deceleration steps in milliseconds
unsigned long lastDecelTime = 0;         // Last time deceleration was applied
bool isDecelerating = false;             // Flag to indicate if we're currently decelerating
byte faultCode = 0;            // Last fault code reported by the inverter

// =====================
// CRC Lookup Table
// =====================
const uint16_t aCrcTab[] = {
  0x0000, 0x1081, 0x2102, 0x3183, 0x4204, 0x5285, 0x6306, 0x7387,
  0x8408, 0x9489, 0xa50a, 0xb58b, 0xc60c, 0xd68d, 0xe70e, 0xf78f
};

// =====================
// Buffer Sizes
// =====================
byte txBuffer[10];
byte rxBuffer[10];

// =====================
// CRC Function
// =====================
uint16_t calcCRC16(byte* buffer, uint16_t length) {
  uint16_t wCrc = 0xffff;
  for (uint16_t i = 0; i < length; i++) {
    wCrc = (wCrc >> 4) ^ aCrcTab[(wCrc & 0x0f) ^ (buffer[i] & 0x0f)];
    wCrc = (wCrc >> 4) ^ aCrcTab[(wCrc & 0x0f) ^ (buffer[i] >> 4)];
  }
  return ~wCrc;
}

// =====================
// Send Command Packet
// =====================
void sendCommand(byte cmd, uint16_t rpm = 0, byte acc = 0) {
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

  DEBUG_SERIAL.print(">> Sent CMD 0x");
  DEBUG_SERIAL.print(cmd, HEX);
  DEBUG_SERIAL.print(" RPM: ");
  DEBUG_SERIAL.println(rpm);

  DEBUG_SERIAL.print("   Bytes: ");
  for (int i = 0; i < 10; i++) {
    DEBUG_SERIAL.print("0x");
    if (txBuffer[i] < 0x10) DEBUG_SERIAL.print("0");
    DEBUG_SERIAL.print(txBuffer[i], HEX);
    DEBUG_SERIAL.print(" ");
  }
  DEBUG_SERIAL.println();
}

// =====================
// Read Response Packet
// =====================
bool readResponse() {
  if (INVERTER_SERIAL.available() >= 10) {
    INVERTER_SERIAL.readBytes(rxBuffer, 10);
    
    DEBUG_SERIAL.print("<< Received: ");
    for (int i = 0; i < 10; i++) {
      DEBUG_SERIAL.print("0x");
      if (rxBuffer[i] < 0x10) DEBUG_SERIAL.print("0");
      DEBUG_SERIAL.print(rxBuffer[i], HEX);
      DEBUG_SERIAL.print(" ");
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
      DEBUG_SERIAL.println("ACK received");
    } else if (rxBuffer[0] == 0x15) {
      DEBUG_SERIAL.println("NCK received");
    } else {
      DEBUG_SERIAL.print("Unknown response code: ");
      DEBUG_SERIAL.println(rxBuffer[0], HEX);
    }

    DEBUG_SERIAL.print("Mode (echoed CMD): 0x"); DEBUG_SERIAL.println(rxBuffer[1], HEX);
    DEBUG_SERIAL.print("Current RPM: ");
    DEBUG_SERIAL.println(actualRPM);
    DEBUG_SERIAL.print("Fault Code: 0x"); DEBUG_SERIAL.println(faultCode, HEX);
    
    return true;
  }
  return false;
}

// =====================
// Send Status Update
// =====================
void sendStatusUpdate() {
  // Format: STATUS:DIRECTION,RPM,ACTUAL_RPM,FAULT
  // Direction is either "CW" or "CCW"
  // Example: STATUS:CW,1200,1198,0
  
  String direction = (currentDirection == CMD_CW) ? "CW" : "CCW";
  
  // Send in a standardized format that's easy to parse
  DEBUG_SERIAL.print("STATUS:");
  DEBUG_SERIAL.print(direction);
  DEBUG_SERIAL.print(",");
  DEBUG_SERIAL.print(currentRPM);
  DEBUG_SERIAL.print(",");
  DEBUG_SERIAL.print(actualRPM);
  DEBUG_SERIAL.print(",");
  DEBUG_SERIAL.println(faultCode);
}

// =====================
// Process Serial Commands
// =====================
void processCommand(String command) {
  command.trim();  // Remove any whitespace
  
  // Don't print debug info for STATUS requests to keep output clean
  if (command != "STATUS") {
    DEBUG_SERIAL.print("Processing command: ");
    DEBUG_SERIAL.println(command);
  }
  
  if (command.startsWith("SETRPM:")) {
    // Extract RPM value after the colon
    String rpmStr = command.substring(7);
    uint16_t rpm = rpmStr.toInt();
    
    // Validate and limit RPM
    if (rpm > 1500) rpm = 1500;  // Maximum safe RPM
    
    // Check if we need to decelerate
    if (rpm < currentRPM) {
      // Set up deceleration
      targetRPM = rpm;
      isDecelerating = true;
      lastDecelTime = millis(); // Start deceleration immediately
      
      DEBUG_SERIAL.print("Starting deceleration from ");
      DEBUG_SERIAL.print(currentRPM);
      DEBUG_SERIAL.print(" to ");
      DEBUG_SERIAL.println(targetRPM);
    } else {
      // For acceleration or same speed, set immediately
      currentRPM = rpm;
      targetRPM = rpm;
      isDecelerating = false;
      
      DEBUG_SERIAL.print("Setting RPM to: ");
      DEBUG_SERIAL.println(currentRPM);
      
      // Send command with current direction and new RPM
      sendCommand(currentDirection, currentRPM);
    }
    
    // Send an immediate status update after setting RPM
    sendStatusUpdate();
  }
  else if (command == "SETCW") {
    currentDirection = CMD_CW;
    DEBUG_SERIAL.println("Setting direction to clockwise");
    
    // Send command with new direction and current RPM
    sendCommand(currentDirection, currentRPM);
    
    // Send an immediate status update after changing direction
    sendStatusUpdate();
  }
  else if (command == "SETCCW") {
    currentDirection = CMD_CCW;
    DEBUG_SERIAL.println("Setting direction to counter-clockwise");
    
    // Send command with new direction and current RPM
    sendCommand(currentDirection, currentRPM);
    
    // Send an immediate status update after changing direction
    sendStatusUpdate();
  }
  else if (command == "STATUS") {
    // Request for current status - respond immediately
    sendStatusUpdate();
  }
  else {
    DEBUG_SERIAL.print("Unknown command: ");
    DEBUG_SERIAL.println(command);
  }
}

// =====================
// Read Serial Input
// =====================
void readSerialInput() {
  while (DEBUG_SERIAL.available() > 0) {
    char inChar = (char)DEBUG_SERIAL.read();
    
    // Process command when newline is received
    if (inChar == '\n' || inChar == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";  // Clear buffer
      }
    } 
    else {
      // Add character to buffer
      inputBuffer += inChar;
    }
  }
}

// =====================
// Setup
// =====================
void setup() {
  DEBUG_SERIAL.begin(115200);
  // while (!DEBUG_SERIAL);
  pinMode(BUILT_IN_LED, OUTPUT);

  // Serial.begin(115200, SERIAL_8N1, 44, 43); // RX0=GPIO44, TX0=GPIO43
  INVERTER_SERIAL.begin(BAUD_RATE, SERIAL_8N1, 44, 43);
  DEBUG_SERIAL.println("Inverter UART Test Starting...");
  DEBUG_SERIAL.println("Ready to accept commands:");
  DEBUG_SERIAL.println("- SETRPM:XX (where XX is RPM value)");
  DEBUG_SERIAL.println("- SETCW (set clockwise rotation)");
  DEBUG_SERIAL.println("- SETCCW (set counter-clockwise rotation)");
  DEBUG_SERIAL.println("- STATUS (request current status)");
  DEBUG_SERIAL.println("");
  DEBUG_SERIAL.println("Status updates format: STATUS:DIRECTION,RPM,ACTUAL_RPM,FAULT");
}

// =====================
// Loop
// =====================
void loop() {
  unsigned long now = millis();
  
  // Read serial input for commands
  readSerialInput();
  
  // Handle deceleration if active
  if (isDecelerating && now - lastDecelTime > DECEL_INTERVAL) {
    lastDecelTime = now;
    
    // Decrease RPM by step size
    if (currentRPM > targetRPM + DECEL_STEP_SIZE) {
      currentRPM -= DECEL_STEP_SIZE;
      DEBUG_SERIAL.print("Decelerating to: ");
      DEBUG_SERIAL.println(currentRPM);
    } else {
      // We've reached or are very close to target
      currentRPM = targetRPM;
      isDecelerating = false;
      DEBUG_SERIAL.print("Reached target RPM: ");
      DEBUG_SERIAL.println(currentRPM);
    }
    
    // Send updated RPM command
    sendCommand(currentDirection, currentRPM);
  }
  
  // Send command every 300ms to maintain motor state
  if (now - lastSendTime > sendInterval) {
    lastSendTime = now;
    
    // Always send command to maintain state
    sendCommand(currentDirection, currentRPM);
    digitalWrite(BUILT_IN_LED, currentState);
    currentState = !currentState;  // Toggle LED state
  }
  
  // Send status update at regular intervals
  if (now - lastStatusTime > statusInterval) {
    lastStatusTime = now;
    sendStatusUpdate();
  }

  // Read response from inverter
  if (readResponse()) {
    // When we successfully get a response with new data, 
    // send an updated status immediately
    sendStatusUpdate();
  }
}