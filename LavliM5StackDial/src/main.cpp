#include <M5Dial.h>
#include "can_comm.h"
#include "lavli_specific_can_comms.h"
#include "provisioning.h"
#include "ui_drawing.h"
#include "mqtt_handler.h"
#include "api_client.h"
#include "config.h"
#include "mac_utils.h"
enum Screen : int { SCREEN_STARTUP = 0, SCREEN_MAIN = 1 };

long lastEnc = LONG_MIN;
Mode currentMode = MODE_WASH;
Mode lastSentMode = MODE_DRY; // Initialize to opposite so first selection sends command
Screen currentScreen = SCREEN_STARTUP;
unsigned long startupStartTime = 0;
const unsigned long STARTUP_DURATION = 3000; // 3 seconds

// CAN communication variables
bool canInitialized = false;

// MQTT and API variables
bool mqttInitialized = false;
bool apiInitialized = false;
MachineStatus currentMachineStatus = MACHINE_IDLE;

void setup() {

  // GPIO TEST MODE - Comment out to run normal CAN code
  // pinMode(GPIO_NUM_1, OUTPUT);
  // pinMode(GPIO_NUM_2, OUTPUT);
  // Serial.begin(115200);
  // delay(1000);
  // Serial.println("=== GPIO PIN TEST MODE ===");
  // Serial.println("Testing GPIO1 and GPIO2...");
  // Serial.println("Watch oscilloscope for square wave on GPIO1 (TX)");
  // return;  // Stop here for pin testing

  auto cfg = M5.config();
  // enableEncoder = true, enableRFID = false
  M5Dial.begin(cfg, true, false);              // initializes device and encoder
  setTextDefaults();  // Set default text properties
  
  // Initialize Serial for debugging
  Serial.begin(115200);
  delay(1000);
  Serial.println("M5Stack Dial - Lavli Controller Starting...");
  
  // Show startup screen
  drawStartupScreen();
  startupStartTime = millis();

  // Initialize CAN communication
  canInitialized = initializeCAN();
  if (canInitialized) {
    Serial.println("CAN communication ready");

    // Initialize sensor data storage system
    initializeSensorStorage();

    // Optional: Uncomment to send test messages on startup
    /*
    Serial.println("\n[TEST] Sending startup test messages...");
    for (int i = 0; i < 5; i++) {
      sendCANMessage(MOTOR_CONTROL_ADDRESS, MOTOR_SET_RPM_CMD, 50 + i * 10);
      delay(200);
    }
    Serial.println("[TEST] Startup test complete\n");
    */
  } else {
    Serial.println("CAN communication failed to initialize");
  }

  // Initialize provisioning
  if (USE_BLE_PROVISIONING) {
    Serial.println("Initializing BLE provisioning...");
    initializeBLE();
  }

  // Try to connect to WiFi
  Serial.println("Setting up WiFi...");
  setupWiFi();

  // Initialize MQTT and API if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected - initializing MQTT and API...");

    // Print MAC address
    String macAddress = getMACAddress();
    Serial.print("Hardware ID (MAC): ");
    Serial.println(macAddress);

    // Initialize MQTT
    initializeMQTT();
    mqttInitialized = true;
    Serial.println("MQTT initialized");

    // Initialize API client
    initializeAPIClient();
    apiInitialized = true;
    Serial.println("API client initialized");
  } else {
    Serial.println("WiFi not connected - skipping MQTT/API initialization");
  }

  // Start encoder at 0 so modulo works cleanly
  M5Dial.Encoder.write(0);
  lastEnc = 0;
}

void loop() {

  // CAN TEST MODE - Comment out to disable continuous testing
  // Uncomment this block to continuously spam CAN messages for testing
  /*
  static unsigned long lastTestMsg = 0;
  if (canInitialized && millis() - lastTestMsg > 500) {
    Serial.println("\n[LOOP TEST] Sending test message...");
    sendCANMessage(MOTOR_CONTROL_ADDRESS, MOTOR_SET_RPM_CMD, random(0, 100));
    lastTestMsg = millis();
  }
  */
  // END TEST MODE

  M5Dial.update();  // required for input updates

  // Handle startup screen timing
  if (currentScreen == SCREEN_STARTUP) {
    if (millis() - startupStartTime >= STARTUP_DURATION) {
      currentScreen = SCREEN_MAIN;
      drawUI(currentMode);
    }
    return; // Don't process encoder during startup
  }

  // Process provisioning
  processProvisioning();

  // Initialize MQTT and API if WiFi just connected
  if (WiFi.status() == WL_CONNECTED && !mqttInitialized) {
    Serial.println("WiFi connected - initializing MQTT and API...");
    initializeMQTT();
    mqttInitialized = true;
    initializeAPIClient();
    apiInitialized = true;
  }

  // Process MQTT if initialized
  if (mqttInitialized) {
    loopMQTT();

    // Process MQTT commands
    if (dryCommand.received) {
      Serial.println("Processing DRY command from MQTT");
      currentMode = MODE_DRY;
      currentMachineStatus = MACHINE_DRYING;
      drawUI(currentMode);

      // Send CAN command if needed
      if (canInitialized) {
        // sendDryCommand(); // Uncomment when CAN commands are defined
      }

      clearMQTTCommand(&dryCommand);
    }

    if (washCommand.received) {
      Serial.println("Processing WASH command from MQTT");
      currentMode = MODE_WASH;
      currentMachineStatus = MACHINE_WASHING;
      drawUI(currentMode);

      // Send CAN command if needed
      if (canInitialized) {
        // sendWashCommand(); // Uncomment when CAN commands are defined
      }

      clearMQTTCommand(&washCommand);
    }

    if (stopCommand.received) {
      Serial.println("Processing STOP command from MQTT");
      currentMachineStatus = MACHINE_IDLE;
      setMachineStatusMessage("Stopped via MQTT");

      // Send CAN stop command if needed
      if (canInitialized) {
        // sendStopCommand(); // Uncomment when CAN commands are defined
      }

      clearMQTTCommand(&stopCommand);
    }
  }

  // Process API check-in if initialized
  if (apiInitialized) {
    // Update machine status based on current mode
    if (currentMachineStatus != MACHINE_IDLE) {
      // Status already set by MQTT command or previous state
      setMachineStatus(currentMachineStatus);
    } else {
      // Infer status from current mode (for manual encoder changes)
      setMachineStatus(MACHINE_IDLE);
    }

    processAPICheckin();
  }

  // Process CAN messages if initialized
  if (canInitialized) {
    receiveCANMessages();
  }

  // Main screen encoder handling
  long enc = M5Dial.Encoder.read();
  if (enc != lastEnc) {
    // Require 4 encoder counts to switch modes (better detent alignment)
    int idx = (int)((enc / 4 % 2 + 2) % 2);  // 0 or 1, handles negatives, divides by 4
    Mode newMode = (idx == 0) ? MODE_WASH : MODE_DRY;
    if (newMode != currentMode) {
      currentMode = newMode;
      M5Dial.Speaker.tone(2400, 30);  // little tick
      drawUI(currentMode);
      
      // Send CAN command if mode changed and CAN is initialized
      // if (canInitialized && newMode != lastSentMode) {
      //   // if (newMode == MODE_WASH) {
      //   //   sendWashCommand();
      //   // } else {
      //   //   sendDryCommand();
      //   // }
      //   lastSentMode = newMode;
      // }
    }
    lastEnc = enc;
  }
  
  // Handle button press to send stop command
  if (M5Dial.BtnA.wasPressed()) {
    M5Dial.Speaker.tone(1800, 50);  // Different tone for button press
    if (canInitialized) {
      Serial.println("Button pressed - sending STOP command");
      // sendStopCommand();
      // sendOutputCommand(CONTROLLER_12V_1_ADDRESS, DEACTIVATE_CMD, 1); // Example: deactivate port 1
      setMotorRPM(25);
    }
  }
}
