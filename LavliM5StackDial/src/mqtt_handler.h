#ifndef MQTT_HANDLER_H
#define MQTT_HANDLER_H

#include <Arduino.h>

// Command structure for MQTT messages
struct MQTTCommand {
    bool received;
    unsigned long timestamp;
};

// External command flags (accessible from main.cpp)
extern MQTTCommand dryCommand;
extern MQTTCommand washCommand;
extern MQTTCommand stopCommand;

/**
 * Initialize MQTT handler
 * Sets up WiFiClientSecure and PubSubClient
 * Must be called after WiFi is connected
 */
void initializeMQTT();

/**
 * Connect to MQTT broker
 * Subscribes to all command topics
 * Returns true if connected, false otherwise
 */
bool connectToMQTT();

/**
 * MQTT loop - must be called frequently in main loop
 * Processes incoming messages and maintains connection
 */
void loopMQTT();

/**
 * Check if MQTT client is connected
 */
bool isMQTTConnected();

/**
 * Process received MQTT commands
 * Resets command flags after processing
 * Should be called from main loop after checking command flags
 */
void clearMQTTCommand(MQTTCommand* cmd);

#endif // MQTT_HANDLER_H
