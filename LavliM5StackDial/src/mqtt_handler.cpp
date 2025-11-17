#include "mqtt_handler.h"
#include "config.h"
#include "mac_utils.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// MQTT client objects
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

// Command flags (accessible from main.cpp)
MQTTCommand dryCommand = {false, 0};
MQTTCommand washCommand = {false, 0};
MQTTCommand stopCommand = {false, 0};

// Topic strings (built dynamically with MAC address)
String topicDry;
String topicWash;
String topicStop;

// MQTT callback function - called when message is received
void onMqttMessage(char* topic, byte* payload, unsigned int length) {
    Serial.print("MQTT message received on topic: ");
    Serial.println(topic);

    // Parse topic to determine command
    String topicStr = String(topic);

    if (topicStr.endsWith("/dry")) {
        dryCommand.received = true;
        dryCommand.timestamp = millis();
        Serial.println("DRY command received");
    }
    else if (topicStr.endsWith("/wash")) {
        washCommand.received = true;
        washCommand.timestamp = millis();
        Serial.println("WASH command received");
    }
    else if (topicStr.endsWith("/stop")) {
        stopCommand.received = true;
        stopCommand.timestamp = millis();
        Serial.println("STOP command received");
    }
}

void initializeMQTT() {
    Serial.println("Initializing MQTT...");

    // Get MAC address for topic construction
    String macAddress = getMACAddress();

    // Build topic strings: /lavli/{hardwareId}/{command}
    topicDry = "/lavli/" + macAddress + "/dry";
    topicWash = "/lavli/" + macAddress + "/wash";
    topicStop = "/lavli/" + macAddress + "/stop";

    Serial.println("MQTT Topics:");
    Serial.println("  DRY:  " + topicDry);
    Serial.println("  WASH: " + topicWash);
    Serial.println("  STOP: " + topicStop);

    // Configure TLS (disable certificate validation like Master Node)
    espClient.setInsecure();

    // Set MQTT server and callback
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(onMqttMessage);

    Serial.println("MQTT initialized");
}

bool connectToMQTT() {
    if (mqttClient.connected()) {
        return true;
    }

    Serial.println("Connecting to MQTT broker...");
    Serial.print("Broker: ");
    Serial.print(MQTT_BROKER);
    Serial.print(":");
    Serial.println(MQTT_PORT);

    // Generate random client ID
    String clientId = "LavliM5Dial-" + String(random(0xffff), HEX);

    // Attempt to connect with username and password
    if (mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.println("MQTT connected!");

        // Subscribe to all command topics
        Serial.println("Subscribing to topics...");

        mqttClient.subscribe(topicDry.c_str());
        Serial.println("  Subscribed: " + topicDry);

        mqttClient.subscribe(topicWash.c_str());
        Serial.println("  Subscribed: " + topicWash);

        mqttClient.subscribe(topicStop.c_str());
        Serial.println("  Subscribed: " + topicStop);

        return true;
    } else {
        Serial.print("MQTT connection failed, rc=");
        Serial.println(mqttClient.state());
        return false;
    }
}

void loopMQTT() {
    // Reconnect if connection is lost
    if (!mqttClient.connected()) {
        static unsigned long lastReconnectAttempt = 0;
        unsigned long now = millis();

        // Try to reconnect every 5 seconds
        if (now - lastReconnectAttempt > 5000) {
            lastReconnectAttempt = now;

            if (connectToMQTT()) {
                lastReconnectAttempt = 0;
            }
        }
    } else {
        // Process incoming messages
        mqttClient.loop();
    }
}

bool isMQTTConnected() {
    return mqttClient.connected();
}

void clearMQTTCommand(MQTTCommand* cmd) {
    cmd->received = false;
    cmd->timestamp = 0;
}
