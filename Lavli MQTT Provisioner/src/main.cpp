#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

#define USE_PROVISIONING false

#define MQTT_BROKER "b-f3e16066-c8b5-48a8-81b0-bc3347afef80-1.mq.us-east-1.amazonaws.com"
#define MQTT_PORT 8883
#define MQTT_TOPIC "/test"
#define MQTT_USERNAME "admin"
#define MQTT_PASSWORD "lavlidevbroker!321"
#define AP_NAME "Lavli-Provisioner"

#define WIFI_SSID "Verizon_ZK3NRK"
#define WIFI_PASSWORD "farm9scope3eddy"

WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
WiFiManager wifiManager;

void setupWiFi();
void configModeCallback(WiFiManager *myWiFiManager);
void saveConfigCallback();
void connectToMQTT();
void onMqttMessage(char* topic, byte* payload, unsigned int length);

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Lavli MQTT Provisioner Starting ===");
  Serial.println("[SETUP] Serial initialized at 115200 baud");
  
  Serial.println("[SETUP] Waiting 1 second before starting...");
  delay(1000);
  
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
}

void loop() {
  if (!mqttClient.connected()) {
    Serial.println("[LOOP] MQTT disconnected, attempting reconnection...");
    connectToMQTT();
  }
  mqttClient.loop();
  delay(10);
}

void setupWiFi() {
  Serial.println("[WIFI] Setting up WiFi...");
  
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
  } else {
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
  }
  
  Serial.println("[WIFI] WiFi connected successfully!");
  Serial.print("[WIFI] IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("[WIFI] MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("[WIFI] Signal strength (RSSI): ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println("AP Name: " + String(AP_NAME));
  Serial.println("IP: " + WiFi.softAPIP().toString());
}

void saveConfigCallback() {
  Serial.println("WiFi configuration saved");
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
    
    String clientId = "LavliProvisioner-" + String(random(0xffff), HEX);
    Serial.print("[MQTT] Client ID: ");
    Serial.println(clientId);
    Serial.print("[MQTT] Username: ");
    Serial.println(MQTT_USERNAME);
    Serial.println("[MQTT] Establishing TLS connection...");
    
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("[MQTT] ✓ Connected successfully!");
      Serial.print("[MQTT] Subscribing to topic: ");
      Serial.println(MQTT_TOPIC);
      
      if (mqttClient.subscribe(MQTT_TOPIC)) {
        Serial.println("[MQTT] ✓ Successfully subscribed to " + String(MQTT_TOPIC));
        Serial.println("[MQTT] Ready to receive messages!");
      } else {
        Serial.println("[MQTT] ✗ Failed to subscribe to " + String(MQTT_TOPIC));
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
  Serial.println("\n[MSG] ===== MESSAGE RECEIVED =====");
  Serial.print("[MSG] Topic: ");
  Serial.println(topic);
  Serial.print("[MSG] Length: ");
  Serial.print(length);
  Serial.println(" bytes");
  Serial.print("[MSG] Data: ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println("[MSG] ========================\n");
}