#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>

#define MQTT_BROKER "broker.hivemq.com"
#define MQTT_PORT 1883
#define MQTT_TOPIC "/lavli"
#define AP_NAME "Lavli-Provisioner"

WiFiClient espClient;
PubSubClient mqttClient(espClient);
WiFiManager wifiManager;

void setupWiFi();
void configModeCallback(WiFiManager *myWiFiManager);
void saveConfigCallback();
void connectToMQTT();
void onMqttMessage(char* topic, byte* payload, unsigned int length);

void setup() {
  Serial.begin(115200);
  Serial.println("Lavli MQTT Provisioner Starting...");
  
  delay(1000);
  
  setupWiFi();
  
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(onMqttMessage);
  
  connectToMQTT();
}

void loop() {
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();
  delay(10);
}

void setupWiFi() {
  Serial.println("Setting up WiFi...");
  
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  if (!wifiManager.autoConnect(AP_NAME)) {
    Serial.println("Failed to connect and hit timeout");
    ESP.restart();
  }
  
  Serial.println("WiFi connected successfully!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
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
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection to ");
    Serial.print(MQTT_BROKER);
    Serial.print(":");
    Serial.println(MQTT_PORT);
    
    String clientId = "LavliProvisioner-" + String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("MQTT connected!");
      Serial.print("Subscribing to topic: ");
      Serial.println(MQTT_TOPIC);
      
      if (mqttClient.subscribe(MQTT_TOPIC)) {
        Serial.println("Successfully subscribed to " + String(MQTT_TOPIC));
      } else {
        Serial.println("Failed to subscribe to " + String(MQTT_TOPIC));
      }
    } else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retrying in 5 seconds");
      delay(5000);
    }
  }
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);
  Serial.print("Data: ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}