#include <Arduino.h>
#include <driver/twai.h>
#include <Adafruit_NeoPixel.h>
#include <ESP32Encoder.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "ble_provisioning.h"

// WiFi Provisioning Configuration
// Options: 0 = Hardcoded credentials, 1 = WiFiManager, 2 = BLE Provisioning
#define PROVISIONING_MODE 2
#define USE_PROVISIONING false
#define MQTT_BROKER "b-f3e16066-c8b5-48a8-81b0-bc3347afef80-1.mq.us-east-1.amazonaws.com"
#define MQTT_PORT 8883
#define MQTT_USERNAME "admin"
#define MQTT_PASSWORD "lavlidevbroker!321"
#define AP_NAME "Lavli-CAN-Master"

#define WIFI_SSID "PKFLetsKickIt"
#define WIFI_PASSWORD "ClarkIsACat"

// MQTT Topics
#define TOPIC_DRY "/lavli/dry"
#define TOPIC_WASH "/lavli/wash"
#define TOPIC_STOP "/lavli/stop"
#define TOPIC_CAN_CONTROL "/lavli/can"

// Interface Setup
#define LED_PIN GPIO_NUM_6
#define LED_COUNT 24
#define ENCODER_A GPIO_NUM_7
#define ENCODER_B GPIO_NUM_8
#define ENCODER_SWITCH GPIO_NUM_9
#define DEBOUNCE_DELAY 50

// Program Timer Configuration
#define PROGRAM_DURATION_MS (10 * 60 * 1000)  // 10 minutes in milliseconds
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
ESP32Encoder encoder;

int lastEncoderValue = 0;
int brightness = 50;
bool lastSwitchState = HIGH;
unsigned long lastDebounceTime = 0;

// Program timing variables
unsigned long programStartTime = 0;
unsigned long programDurationMs = PROGRAM_DURATION_MS;
bool fadeInActive = false;
unsigned long fadeStartTime = 0;
#define FADE_DURATION_MS 2000

// WiFi and MQTT clients
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
WiFiManager wifiManager;

// MQTT Command Variables
struct MQTTCommand {
  bool received;
  int value;
  unsigned long timestamp;
};

MQTTCommand dryCommand = {false, 0, 0};
MQTTCommand washCommand = {false, 0, 0};
MQTTCommand stopCommand = {false, 0, 0};

// Generic CAN Command Structure
struct GenericCANCommand {
  bool received;
  uint16_t address;
  uint8_t data[8];
  uint8_t data_length;
  unsigned long timestamp;
};

GenericCANCommand genericCANCommand = {false, 0, {0}, 0, 0};

// CAN IDs of Controllers
#define CONTROLLER_120V_ADDRESS 0x543
#define CONTROLLER_MOTOR_ADDRESS 0x311

// CAN pins
#define CAN_TX_PIN GPIO_NUM_4
#define CAN_RX_PIN GPIO_NUM_5  

// Command definitions for output control
#define ACTIVATE_CMD   0x01
#define DEACTIVATE_CMD 0x02

// New command definitions for sensor requests
#define READ_ANALOG_CMD   0x03
#define READ_DIGITAL_CMD  0x04
#define READ_ALL_ANALOG_CMD 0x05
#define READ_ALL_DIGITAL_CMD 0x06

// Motor command definitions (matching motor control node)
#define MOTOR_SET_RPM_CMD     0x30
#define MOTOR_SET_DIRECTION_CMD 0x31
#define MOTOR_STOP_CMD        0x32
#define MOTOR_STATUS_CMD      0x33

// Response command definitions
#define ACK_ACTIVATE      0x10
#define ACK_DEACTIVATE    0x11
#define ANALOG_DATA       0x20
#define DIGITAL_DATA      0x21
#define ALL_ANALOG_DATA   0x22
#define ALL_DIGITAL_DATA  0x23
#define ACK_MOTOR_RPM         0x40
#define ACK_MOTOR_DIRECTION   0x41
#define ACK_MOTOR_STOP        0x42
#define MOTOR_STATUS_DATA     0x43
#define ERROR_RESPONSE    0xFF

// Sensor data structure for storing received values
struct SensorReading {
  uint16_t analog_value;
  bool digital_value;
  bool valid;
  unsigned long timestamp;
};

// Storage for sensor readings from different devices
#define MAX_DEVICES 16
#define MAX_PINS 8
SensorReading analog_readings[MAX_DEVICES][MAX_PINS];
SensorReading digital_readings[MAX_DEVICES][MAX_PINS];

// CAN configuration
twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_PIN, CAN_RX_PIN, TWAI_MODE_NORMAL);
twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

// Function prototypes
void setupWiFi();
void configModeCallback(WiFiManager *myWiFiManager);
void saveConfigCallback();
void connectToMQTT();
void onMqttMessage(char* topic, byte* payload, unsigned int length);
void processMQTTCommands();
bool parseGenericCANCommand(String jsonMessage);
bool sendGenericCANMessage(uint16_t address, uint8_t* data, uint8_t data_length);

bool initializeCAN();
void initializeSensorStorage();
bool sendOutputCommand(uint16_t device_address, uint8_t command, uint8_t port);
bool requestAnalogReading(uint16_t device_address, uint8_t pin);
bool requestDigitalReading(uint16_t device_address, uint8_t pin);
bool requestAllAnalogReadings(uint16_t device_address);
bool requestAllDigitalReadings(uint16_t device_address);
bool sendMotorRPM(uint16_t device_address, uint16_t rpm);
bool sendMotorDirection(uint16_t device_address, bool clockwise);
bool sendMotorStop(uint16_t device_address);
bool requestMotorStatus(uint16_t device_address);
void receiveCANMessages();
void processReceivedMessage(twai_message_t* message);
void storeSensorReading(uint16_t device_address, uint8_t pin, uint16_t value, bool is_analog);
SensorReading getAnalogReading(uint16_t device_address, uint8_t pin);
SensorReading getDigitalReading(uint16_t device_address, uint8_t pin);
void printSensorData(uint16_t device_address);
uint8_t getDeviceIndex(uint16_t device_address);

enum MachineState {
  STATE_OFF,
  STATE_IDLE,
  STATE_SELECT_DRY,
  STATE_SELECT_WASH,
  STATE_DRYING,
  STATE_WASHING,
};


MachineState currentState = STATE_IDLE;

void startWash()
{
    sendMotorRPM(CONTROLLER_MOTOR_ADDRESS, 50);
    programStartTime = millis();
    currentState = STATE_WASHING;
}

void startDry()
{
    sendOutputCommand(CONTROLLER_120V_ADDRESS, ACTIVATE_CMD, 0);
    sendMotorRPM(CONTROLLER_MOTOR_ADDRESS, 50);
    programStartTime = millis();
    currentState = STATE_DRYING;
}

void stopAll()
{
    sendMotorRPM(CONTROLLER_MOTOR_ADDRESS, 0);
    sendOutputCommand(CONTROLLER_120V_ADDRESS, DEACTIVATE_CMD, 0);
    sendOutputCommand(CONTROLLER_120V_ADDRESS, DEACTIVATE_CMD, 1);
    programStartTime = 0;
    currentState = STATE_IDLE;
}

bool isProgramTimeElapsed() {
    if (programStartTime == 0) return false;
    return (millis() - programStartTime) >= programDurationMs;
}

void checkProgramTimer() {
    if ((currentState == STATE_WASHING || currentState == STATE_DRYING) && isProgramTimeElapsed()) {
        Serial.println("[TIMER] Program duration completed, stopping all operations");
        stopAll();
    }
}


void setupInterface()
{
  strip.begin();
  strip.show();
  strip.setBrightness(brightness);
  
  ESP32Encoder::useInternalWeakPullResistors = puType::UP;
  encoder.attachHalfQuad(ENCODER_A, ENCODER_B);
  encoder.clearCount();

  pinMode(ENCODER_SWITCH, INPUT_PULLUP);

  for(int i = 0; i < LED_COUNT; i++){
    strip.setPixelColor(i, strip.Color(0, 0, 255));
  }
  strip.show();
}

void handleSwitchPress() {
  bool switchReading = digitalRead(ENCODER_SWITCH);
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (switchReading != lastSwitchState) {
      if (switchReading == LOW) {
        // TODO: Do Switch functions in here
        Serial.println("[SWITCH] Button pressed");

        if(currentState == STATE_OFF) {
          currentState = STATE_IDLE;
          fadeInActive = true;
          fadeStartTime = millis();
        }
        else if(currentState == STATE_SELECT_WASH){
          startWash();
        }
        else if(currentState == STATE_SELECT_DRY){
          startDry();
        }
        else if(currentState == STATE_WASHING || currentState == STATE_DRYING){
          stopAll();
        }

      }
      lastSwitchState = switchReading;
    }
    lastDebounceTime = millis();
  }
}

void drawLEDs() {
  // Handle fade-in animation
  if (fadeInActive) {
    unsigned long fadeElapsed = millis() - fadeStartTime;
    if (fadeElapsed >= FADE_DURATION_MS) {
      fadeInActive = false;
    } else {
      float fadeProgress = (float)fadeElapsed / FADE_DURATION_MS;
      uint8_t alpha = (uint8_t)(255 * fadeProgress);
      
      for(int i = 0; i < LED_COUNT; i++){
        strip.setPixelColor(i, strip.Color(alpha, alpha, alpha));
      }
      strip.show();
      return;
    }
  }
  
  if(currentState == STATE_OFF) {
    for(int i = 0; i < LED_COUNT; i++){
      strip.setPixelColor(i, strip.Color(0, 0, 0));
    }
  }
  else if(currentState == STATE_IDLE){
    for(int i = 0; i < LED_COUNT; i++){
      strip.setPixelColor(i, strip.Color(255, 255, 255));
    }
  }
  else if(currentState == STATE_SELECT_WASH){
    for(int i = 0; i < LED_COUNT; i++){
      strip.setPixelColor(i, strip.Color(0, 255, 0));
    }
  }
  else if(currentState == STATE_SELECT_DRY){
    for(int i = 0; i < LED_COUNT; i++){
      strip.setPixelColor(i, strip.Color(255, 0, 0));
    }
  }
  else if(currentState == STATE_DRYING || currentState == STATE_WASHING){
    // Calculate remaining time and LEDs to show
    if (programStartTime > 0) {
      unsigned long elapsed = millis() - programStartTime;
      unsigned long remaining = (elapsed >= programDurationMs) ? 0 : (programDurationMs - elapsed);
      float remainingPercent = (float)remaining / programDurationMs;
      int ledsToShow = (int)(LED_COUNT * remainingPercent);
      
      // Set color based on program type
      uint32_t color;
      if (currentState == STATE_DRYING) {
        color = strip.Color(255, 100, 0); // Orange for drying
      } else {
        color = strip.Color(0, 100, 255); // Blue for washing
      }
      
      // Light up LEDs for remaining time
      for(int i = 0; i < LED_COUNT; i++){
        if (i < ledsToShow) {
          strip.setPixelColor(i, color);
        } else {
          strip.setPixelColor(i, strip.Color(0, 0, 0));
        }
      }
    }
  }

  strip.show();
}

void readEncoder(){
  int newValue = encoder.getCount();
  newValue = newValue /4;
  if (newValue != lastEncoderValue) {
    
    if(currentState == STATE_IDLE || currentState == STATE_SELECT_DRY){
      currentState = STATE_SELECT_WASH;
    }
    else if(currentState == STATE_SELECT_WASH){
      currentState = STATE_SELECT_DRY;
    }

    lastEncoderValue = newValue;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Lavli CAN Master with MQTT Starting ===");
  Serial.println("[SETUP] Serial initialized at 115200 baud");
  
  delay(2000);

  setupInterface();
  Serial.println("[SETUP] Interface initialized");
  
  // Initialize sensor data storage
  initializeSensorStorage();
  Serial.println("[SETUP] Sensor storage initialized");
  
  // Initialize CAN
  if (initializeCAN()) {
    Serial.println("[SETUP] CAN initialized successfully");
  } else {
    Serial.println("[SETUP] CAN initialization failed");
  }
  
  Serial.println("[SETUP] Starting WiFi setup...");
  setupWiFi();
  Serial.println("[SETUP] WiFi setup complete");
  
  Serial.println("[SETUP] Configuring TLS client (insecure mode)");
  espClient.setInsecure();
  
  Serial.print("[SETUP] Setting MQTT server: ");
  Serial.print(MQTT_BROKER);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  
  Serial.println("[SETUP] Setting MQTT callback function");
  mqttClient.setCallback(onMqttMessage);
  
  Serial.println("[SETUP] Attempting initial MQTT connection...");
  connectToMQTT();
  
  Serial.println("[SETUP] Setup complete!");
  Serial.println("Commands:");
  Serial.println("  activate <addr> <port>    - Activate output port");
  Serial.println("  deactivate <addr> <port>  - Deactivate output port");
  Serial.println("  analog <addr> <pin>       - Read analog pin");
  Serial.println("  digital <addr> <pin>      - Read digital pin");
  Serial.println("  all_analog <addr>         - Read all analog pins");
  Serial.println("  all_digital <addr>        - Read all digital pins");
  Serial.println("  status <addr>             - Show sensor data for device");
  Serial.println("  motor_rpm <addr> <rpm>    - Set motor RPM (0-1500)");
  Serial.println("  motor_direction <addr> <0|1> - Set motor direction (0=CCW, 1=CW)");
  Serial.println("  motor_stop <addr>         - Stop motor");
  Serial.println("  motor_status <addr>       - Request motor status");
  Serial.println("MQTT Topics subscribed:");
  Serial.println("  " + String(TOPIC_DRY) + " - Dry command");
  Serial.println("  " + String(TOPIC_WASH) + " - Wash command");
  Serial.println("  " + String(TOPIC_STOP) + " - Stop command");
  Serial.println("  " + String(TOPIC_CAN_CONTROL) + " - Generic CAN control (JSON)");
  Serial.println();
  Serial.println("Generic CAN JSON format:");
  Serial.println("  {\"address\":\"0x311\", \"data\":[\"0x30\", 50, 0]}");
  Serial.println("  {\"address\":785, \"data\":[1, 2]}");
  Serial.println();
}

void doSerialControl()
{
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    // Parse command
    int space1 = command.indexOf(' ');
    int space2 = command.lastIndexOf(' ');
    
    if (space1 > 0) {
      String cmd = command.substring(0, space1);
      String addr_str = command.substring(space1 + 1, space2 > space1 ? space2 : command.length());
      String param_str = space2 > space1 ? command.substring(space2 + 1) : "";
      
      uint16_t device_addr = strtol(addr_str.c_str(), NULL, 16);
      uint8_t param = param_str.length() > 0 ? param_str.toInt() : 0;
      
      // Execute commands
      if (cmd == "activate") {
        if (param_str.length() > 0) {
          Serial.printf("Sending activate command to 0x%03X, port %d\n", device_addr, param);
          sendOutputCommand(device_addr, ACTIVATE_CMD, param);
        } else {
          Serial.println("Usage: activate <addr> <port>");
        }
      }
      else if (cmd == "deactivate") {
        if (param_str.length() > 0) {
          Serial.printf("Sending deactivate command to 0x%03X, port %d\n", device_addr, param);
          sendOutputCommand(device_addr, DEACTIVATE_CMD, param);
        } else {
          Serial.println("Usage: deactivate <addr> <port>");
        }
      }
      else if (cmd == "analog") {
        if (param_str.length() > 0) {
          Serial.printf("Requesting analog reading from 0x%03X, pin %d\n", device_addr, param);
          requestAnalogReading(device_addr, param);
        } else {
          Serial.println("Usage: analog <addr> <pin>");
        }
      }
      else if (cmd == "digital") {
        if (param_str.length() > 0) {
          Serial.printf("Requesting digital reading from 0x%03X, pin %d\n", device_addr, param);
          requestDigitalReading(device_addr, param);
        } else {
          Serial.println("Usage: digital <addr> <pin>");
        }
      }
      else if (cmd == "all_analog") {
        Serial.printf("Requesting all analog readings from 0x%03X\n", device_addr);
        requestAllAnalogReadings(device_addr);
      }
      else if (cmd == "all_digital") {
        Serial.printf("Requesting all digital readings from 0x%03X\n", device_addr);
        requestAllDigitalReadings(device_addr);
      }
      else if (cmd == "status") {
        printSensorData(device_addr);
      }
      else if (cmd == "motor_rpm") {
        if (param_str.length() > 0) {
          Serial.printf("Setting motor RPM to %d on device 0x%03X\n", param, device_addr);
          sendMotorRPM(device_addr, param);
        } else {
          Serial.println("Usage: motor_rpm <addr> <rpm>");
        }
      }
      else if (cmd == "motor_direction") {
        if (param_str.length() > 0) {
          bool clockwise = (param != 0);
          Serial.printf("Setting motor direction to %s on device 0x%03X\n", 
                        clockwise ? "CW" : "CCW", device_addr);
          sendMotorDirection(device_addr, clockwise);
        } else {
          Serial.println("Usage: motor_direction <addr> <0=CCW|1=CW>");
        }
      }
      else if (cmd == "motor_stop") {
        Serial.printf("Stopping motor on device 0x%03X\n", device_addr);
        sendMotorStop(device_addr);
      }
      else if (cmd == "motor_status") {
        Serial.printf("Requesting motor status from device 0x%03X\n", device_addr);
        requestMotorStatus(device_addr);
      }
      else {
        Serial.println("Unknown command");
      }
    }
  }
}

void loop() {
#if PROVISIONING_MODE == 2
  // Process BLE provisioning state machine
  BLEProvisioning::process();

  // Only proceed with MQTT and other operations if WiFi is connected
  if (!BLEProvisioning::isWiFiConnected()) {
    // While waiting for WiFi, still handle UI
    handleSwitchPress();
    readEncoder();
    drawLEDs();
    delay(10);
    return;
  }
#endif

  // Handle MQTT connection
  if (!mqttClient.connected()) {
    Serial.println("[LOOP] MQTT disconnected, attempting reconnection...");
    connectToMQTT();
  }
  mqttClient.loop();

  // Process MQTT commands
  processMQTTCommands();

  // Handle serial commands
  doSerialControl();

  // Process incoming CAN messages
  receiveCANMessages();

  // Check program timer
  checkProgramTimer();

  handleSwitchPress();
  readEncoder();
  drawLEDs();
  delay(10);
}

void setupWiFi() {
  Serial.println("[WIFI] Setting up WiFi...");

#if PROVISIONING_MODE == 2
  // BLE Provisioning Mode
  Serial.println("[WIFI] Using BLE provisioning mode");
  Serial.println("[WIFI] Starting BLE provisioning service...");
  BLEProvisioning::begin("Lavli Master Node");
  Serial.println("[WIFI] BLE provisioning active - use BLE app to configure WiFi");
  Serial.println("[WIFI] WiFi will connect after credentials are provided via BLE");
  // WiFi connection will happen asynchronously in loop() via BLEProvisioning::process()
  return; // Don't wait for WiFi here in BLE mode

#elif PROVISIONING_MODE == 1
  // WiFiManager Mode
  if (USE_PROVISIONING) {
    Serial.println("[WIFI] Using WiFiManager provisioning mode");
    wifiManager.setAPCallback(configModeCallback);
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    Serial.println("[WIFI] Starting WiFiManager autoConnect...");
    if (!wifiManager.autoConnect(AP_NAME)) {
      Serial.println("[WIFI] ERROR: Failed to connect and hit timeout");
      Serial.println("[WIFI] Restarting ESP32...");
      ESP.restart();
    }
  }

#else
  // Hardcoded Credentials Mode (Mode 0 or default)
  Serial.println("[WIFI] Using hardcoded credentials mode");
  Serial.print("[WIFI] SSID: ");
  Serial.println(WIFI_SSID);
  Serial.println("[WIFI] Starting WiFi connection...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("[WIFI] Connecting to ");
  Serial.print(WIFI_SSID);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 60) {
      Serial.println("");
      Serial.println("[WIFI] ERROR: Connection timeout after 30 seconds");
      Serial.println("[WIFI] Restarting ESP32...");
      ESP.restart();
    }
  }
  Serial.println("");
#endif

  // Only print connection info if we're connected at this point
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] WiFi connected successfully!");
    Serial.print("[WIFI] IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WIFI] MAC address: ");
    Serial.println(WiFi.macAddress());
    Serial.print("[WIFI] Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  }
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("[WIFI] Entered config mode");
  Serial.println("[WIFI] AP Name: " + String(AP_NAME));
  Serial.println("[WIFI] IP: " + WiFi.softAPIP().toString());
}

void saveConfigCallback() {
  Serial.println("[WIFI] WiFi configuration saved");
}

void connectToMQTT() {
  int attempts = 0;
  while (!mqttClient.connected()) {
    attempts++;
    Serial.print("[MQTT] Attempt #");
    Serial.print(attempts);
    Serial.print(" - Connecting to ");
    Serial.print(MQTT_BROKER);
    Serial.print(":");
    Serial.println(MQTT_PORT);
    
    String clientId = "LavliCANMaster-" + String(random(0xffff), HEX);
    Serial.print("[MQTT] Client ID: ");
    Serial.println(clientId);
    Serial.print("[MQTT] Username: ");
    Serial.println(MQTT_USERNAME);
    Serial.println("[MQTT] Establishing TLS connection...");
    
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("[MQTT] ✓ Connected successfully!");
      
      // Subscribe to all command topics
      Serial.print("[MQTT] Subscribing to topics...");
      bool allSubscribed = true;
      
      if (mqttClient.subscribe(TOPIC_DRY)) {
        Serial.print(" " + String(TOPIC_DRY) + " ✓");
      } else {
        Serial.print(" " + String(TOPIC_DRY) + " ✗");
        allSubscribed = false;
      }
      
      if (mqttClient.subscribe(TOPIC_WASH)) {
        Serial.print(" " + String(TOPIC_WASH) + " ✓");
      } else {
        Serial.print(" " + String(TOPIC_WASH) + " ✗");
        allSubscribed = false;
      }
      
      if (mqttClient.subscribe(TOPIC_STOP)) {
        Serial.print(" " + String(TOPIC_STOP) + " ✓");
      } else {
        Serial.print(" " + String(TOPIC_STOP) + " ✗");
        allSubscribed = false;
      }
      
      if (mqttClient.subscribe(TOPIC_CAN_CONTROL)) {
        Serial.print(" " + String(TOPIC_CAN_CONTROL) + " ✓");
      } else {
        Serial.print(" " + String(TOPIC_CAN_CONTROL) + " ✗");
        allSubscribed = false;
      }
      
      Serial.println();
      
      if (allSubscribed) {
        Serial.println("[MQTT] ✓ Successfully subscribed to all topics!");
        Serial.println("[MQTT] Ready to receive commands!");
      } else {
        Serial.println("[MQTT] ✗ Failed to subscribe to some topics");
      }
    } else {
      Serial.print("[MQTT] ✗ Connection failed, error code: ");
      Serial.print(mqttClient.state());
      Serial.println(" (see PubSubClient.h for error codes)");
      Serial.println("[MQTT] Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  Serial.println("\n[MQTT] ===== MESSAGE RECEIVED =====");
  Serial.print("[MQTT] Topic: ");
  Serial.println(topic);
  Serial.print("[MQTT] Length: ");
  Serial.print(length);
  Serial.println(" bytes");
  Serial.print("[MQTT] Data: ");
  
  // Convert payload to string
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  // Parse the numeric value from the message
  int value = message.toInt();
  Serial.print("[MQTT] Parsed value: ");
  Serial.println(value);
  
  // Store the command based on topic
  if (strcmp(topic, TOPIC_DRY) == 0) {
    dryCommand.received = true;
    dryCommand.value = value;
    dryCommand.timestamp = millis();
    Serial.println("[MQTT] Dry command received");
  }
  else if (strcmp(topic, TOPIC_WASH) == 0) {
    washCommand.received = true;
    washCommand.value = value;
    washCommand.timestamp = millis();
    Serial.println("[MQTT] Wash command received");
  }
  else if (strcmp(topic, TOPIC_STOP) == 0) {
    stopCommand.received = true;
    stopCommand.value = value;
    stopCommand.timestamp = millis();
    Serial.println("[MQTT] Stop command received");
  }
  else if (strcmp(topic, TOPIC_CAN_CONTROL) == 0) {
    Serial.println("[MQTT] Generic CAN command received");
    if (parseGenericCANCommand(message)) {
      Serial.println("[MQTT] CAN command parsed successfully");
    } else {
      Serial.println("[MQTT] Failed to parse CAN command");
    }
  }
  
  Serial.println("[MQTT] ========================\n");
}



void processMQTTCommands() {
  // Process dry command
  if (dryCommand.received) {
    Serial.println("[COMMAND] Processing DRY command");
    Serial.printf("[COMMAND] Dry value: %d\n", dryCommand.value);
    Serial.printf("[COMMAND] Starting dry cycle for %lu minutes\n", programDurationMs / 60000);
    
    startDry();
    
    // Reset the command flag
    dryCommand.received = false;
  }
  
  // Process wash command
  if (washCommand.received) {
    Serial.println("[COMMAND] Processing WASH command");
    Serial.printf("[COMMAND] Wash value: %d\n", washCommand.value);
    Serial.printf("[COMMAND] Starting wash cycle for %lu minutes\n", programDurationMs / 60000);
    
    startWash();
    
    // Reset the command flag
    washCommand.received = false;
  }
  
  // Process stop command
  if (stopCommand.received) {
    Serial.println("[COMMAND] Processing STOP command");
    Serial.printf("[COMMAND] Stop value: %d\n", stopCommand.value);
    
    stopAll();
    
    // Reset the command flag
    stopCommand.received = false;
  }
  
  // Process generic CAN command
  if (genericCANCommand.received) {
    Serial.println("[COMMAND] Processing GENERIC CAN command");
    Serial.printf("[COMMAND] Address: 0x%03X, Data Length: %d\n", genericCANCommand.address, genericCANCommand.data_length);
    Serial.print("[COMMAND] Data: ");
    for (int i = 0; i < genericCANCommand.data_length; i++) {
      Serial.printf("0x%02X ", genericCANCommand.data[i]);
    }
    Serial.println();
    
    sendGenericCANMessage(genericCANCommand.address, genericCANCommand.data, genericCANCommand.data_length);
    
    // Reset the command flag
    genericCANCommand.received = false;
  }
}

bool initializeCAN() {
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    Serial.println("Failed to install TWAI driver");
    return false;
  }
  
  if (twai_start() != ESP_OK) {
    Serial.println("Failed to start TWAI driver");
    return false;
  }
  
  return true;
}

void initializeSensorStorage() {
  for (int dev = 0; dev < MAX_DEVICES; dev++) {
    for (int pin = 0; pin < MAX_PINS; pin++) {
      analog_readings[dev][pin].valid = false;
      analog_readings[dev][pin].analog_value = 0;
      analog_readings[dev][pin].timestamp = 0;
      
      digital_readings[dev][pin].valid = false;
      digital_readings[dev][pin].digital_value = false;
      digital_readings[dev][pin].timestamp = 0;
    }
  }
}

bool sendOutputCommand(uint16_t device_address, uint8_t command, uint8_t port) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = command;
  message.data[1] = port;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool requestAnalogReading(uint16_t device_address, uint8_t pin) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = READ_ANALOG_CMD;
  message.data[1] = pin;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool requestDigitalReading(uint16_t device_address, uint8_t pin) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = READ_DIGITAL_CMD;
  message.data[1] = pin;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool requestAllAnalogReadings(uint16_t device_address) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 1;
  message.data[0] = READ_ALL_ANALOG_CMD;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool requestAllDigitalReadings(uint16_t device_address) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 1;
  message.data[0] = READ_ALL_DIGITAL_CMD;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

void receiveCANMessages() {
  twai_message_t message;
  
  if (twai_receive(&message, 0) == ESP_OK) {
    Serial.printf("Received from 0x%03X: ", message.identifier);
    for (int i = 0; i < message.data_length_code; i++) {
      Serial.printf("0x%02X ", message.data[i]);
    }
    Serial.println();
    
    processReceivedMessage(&message);
  }
}

void processReceivedMessage(twai_message_t* message) {
  if (message->data_length_code < 1) return;
  
  uint8_t response_type = message->data[0];
  
  switch (response_type) {
    case ACK_ACTIVATE:
    case ACK_DEACTIVATE:
      if (message->data_length_code >= 3) {
        Serial.printf("Output command acknowledged: Port %d, Status 0x%02X\n", 
                      message->data[1], message->data[2]);
      }
      break;
      
    case ACK_MOTOR_RPM:
      if (message->data_length_code >= 4) {
        uint16_t confirmed_speed = (message->data[1] << 8) | message->data[2];
        Serial.printf("Motor speed command acknowledged: Speed %d, Status 0x%02X\n", 
                      confirmed_speed, message->data[3]);
      }
      break;
      
    case ANALOG_DATA:
      if (message->data_length_code >= 5) {
        uint8_t pin = message->data[1];
        uint16_t value = (message->data[2] << 8) | message->data[3];
        Serial.printf("Analog pin %d: %d (%.2fV)\n", pin, value, value * 3.3 / 4095.0);
        storeSensorReading(message->identifier, pin, value, true);
      }
      break;
      
    case DIGITAL_DATA:
      if (message->data_length_code >= 3) {
        uint8_t pin = message->data[1];
        bool value = message->data[2] != 0;
        Serial.printf("Digital pin %d: %s\n", pin, value ? "HIGH" : "LOW");
        storeSensorReading(message->identifier, pin, value ? 1 : 0, false);
      }
      break;
      
    case ALL_ANALOG_DATA:
      // Process multiple analog readings in one message
      for (int i = 1; i < message->data_length_code; i += 3) {
        if (i + 2 < message->data_length_code) {
          uint8_t pin = message->data[i];
          uint16_t value = (message->data[i+1] << 8) | message->data[i+2];
          Serial.printf("Analog pin %d: %d (%.2fV)\n", pin, value, value * 3.3 / 4095.0);
          storeSensorReading(message->identifier, pin, value, true);
        }
      }
      break;
      
    case ALL_DIGITAL_DATA:
      // Process multiple digital readings packed in bytes
      if (message->data_length_code >= 2) {
        uint8_t digital_data = message->data[1];
        for (int pin = 0; pin < 8; pin++) {
          bool value = (digital_data >> pin) & 1;
          Serial.printf("Digital pin %d: %s\n", pin, value ? "HIGH" : "LOW");
          storeSensorReading(message->identifier, pin, value ? 1 : 0, false);
        }
      }
      break;
      
    case ERROR_RESPONSE:
      if (message->data_length_code >= 3) {
        Serial.printf("Error response: Port/Pin %d, Error code 0x%02X\n", 
                      message->data[1], message->data[2]);
      }
      break;
  }
}

void storeSensorReading(uint16_t device_address, uint8_t pin, uint16_t value, bool is_analog) {
  uint8_t dev_index = getDeviceIndex(device_address);
  if (dev_index >= MAX_DEVICES || pin >= MAX_PINS) return;
  
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
  uint8_t dev_index = getDeviceIndex(device_address);
  if (dev_index < MAX_DEVICES && pin < MAX_PINS) {
    return analog_readings[dev_index][pin];
  }
  SensorReading invalid = {0, false, false, 0};
  return invalid;
}

SensorReading getDigitalReading(uint16_t device_address, uint8_t pin) {
  uint8_t dev_index = getDeviceIndex(device_address);
  if (dev_index < MAX_DEVICES && pin < MAX_PINS) {
    return digital_readings[dev_index][pin];
  }
  SensorReading invalid = {0, false, false, 0};
  return invalid;
}

void printSensorData(uint16_t device_address) {
  uint8_t dev_index = getDeviceIndex(device_address);
  if (dev_index >= MAX_DEVICES) {
    Serial.println("Invalid device address");
    return;
  }
  
  Serial.printf("\n=== Sensor Data for Device 0x%03X ===\n", device_address);
  
  Serial.println("Analog Pins:");
  bool found_analog = false;
  for (int pin = 0; pin < MAX_PINS; pin++) {
    if (analog_readings[dev_index][pin].valid) {
      unsigned long age = millis() - analog_readings[dev_index][pin].timestamp;
      Serial.printf("  Pin %d: %d (%.2fV) - %lu ms ago\n", 
                    pin, 
                    analog_readings[dev_index][pin].analog_value,
                    analog_readings[dev_index][pin].analog_value * 3.3 / 4095.0,
                    age);
      found_analog = true;
    }
  }
  if (!found_analog) Serial.println("  No analog data");
  
  Serial.println("Digital Pins:");
  bool found_digital = false;
  for (int pin = 0; pin < MAX_PINS; pin++) {
    if (digital_readings[dev_index][pin].valid) {
      unsigned long age = millis() - digital_readings[dev_index][pin].timestamp;
      Serial.printf("  Pin %d: %s - %lu ms ago\n", 
                    pin, 
                    digital_readings[dev_index][pin].digital_value ? "HIGH" : "LOW",
                    age);
      found_digital = true;
    }
  }
  if (!found_digital) Serial.println("  No digital data");
  
  Serial.println("================================\n");
}

uint8_t getDeviceIndex(uint16_t device_address) {
  // Simple mapping - you might want a more sophisticated approach
  return device_address % MAX_DEVICES;
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
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool sendMotorDirection(uint16_t device_address, bool clockwise) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 2;
  message.data[0] = MOTOR_SET_DIRECTION_CMD;
  message.data[1] = clockwise ? 1 : 0;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool sendMotorStop(uint16_t device_address) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 1;
  message.data[0] = MOTOR_STOP_CMD;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool requestMotorStatus(uint16_t device_address) {
  twai_message_t message;
  
  message.identifier = device_address;
  message.extd = 0;
  message.rtr = 0;
  message.data_length_code = 1;
  message.data[0] = MOTOR_STATUS_CMD;
  
  return twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
}

bool parseGenericCANCommand(String jsonMessage) {
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, jsonMessage);
  
  if (error) {
    Serial.print("[JSON] Parse failed: ");
    Serial.println(error.c_str());
    return false;
  }
  
  // Parse address (supports hex strings like "0x311" or decimal)
  if (!doc.containsKey("address")) {
    Serial.println("[JSON] Missing 'address' field");
    return false;
  }
  
  String addressStr = doc["address"];
  uint16_t address;
  if (addressStr.startsWith("0x") || addressStr.startsWith("0X")) {
    address = (uint16_t)strtol(addressStr.c_str(), NULL, 16);
  } else {
    address = doc["address"].as<uint16_t>();
  }
  
  // Parse data array
  if (!doc.containsKey("data") || !doc["data"].is<JsonArray>()) {
    Serial.println("[JSON] Missing or invalid 'data' array");
    return false;
  }
  
  JsonArray dataArray = doc["data"];
  uint8_t dataLength = dataArray.size();
  
  if (dataLength > 8) {
    Serial.println("[JSON] Data array too large (max 8 bytes)");
    return false;
  }
  
  // Copy data to command structure
  genericCANCommand.address = address;
  genericCANCommand.data_length = dataLength;
  genericCANCommand.timestamp = millis();
  
  for (uint8_t i = 0; i < dataLength; i++) {
    if (dataArray[i].is<String>()) {
      String byteStr = dataArray[i];
      if (byteStr.startsWith("0x") || byteStr.startsWith("0X")) {
        genericCANCommand.data[i] = (uint8_t)strtol(byteStr.c_str(), NULL, 16);
      } else {
        genericCANCommand.data[i] = byteStr.toInt();
      }
    } else {
      genericCANCommand.data[i] = dataArray[i].as<uint8_t>();
    }
  }
  
  genericCANCommand.received = true;
  
  Serial.printf("[JSON] Parsed CAN command: Address=0x%03X, Length=%d\n", address, dataLength);
  return true;
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
  
  bool result = twai_transmit(&message, pdMS_TO_TICKS(1000)) == ESP_OK;
  
  if (result) {
    Serial.printf("[CAN] Generic message sent to 0x%03X: ", address);
    for (uint8_t i = 0; i < data_length; i++) {
      Serial.printf("0x%02X ", data[i]);
    }
    Serial.println();
  } else {
    Serial.printf("[CAN] Failed to send message to 0x%03X\n", address);
  }
  
  return result;
}