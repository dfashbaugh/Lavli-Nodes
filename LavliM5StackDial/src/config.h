#ifndef CONFIG_H
#define CONFIG_H

// ============================================================================
// API Check-in Configuration
// ============================================================================

// Base URL for API server (change this to your server address)
#define BASE_URL "https://your-api-server.com"

// Enable/disable API check-in functionality
// Set to false to completely disable API check-ins at compile time
#define ENABLE_API_CHECKIN true

// API check-in interval in milliseconds (2 seconds)
#define API_CHECKIN_INTERVAL_MS 2000

// API endpoint path
#define API_CHECKIN_ENDPOINT "/api/machines/check-in"

// ============================================================================
// MQTT Configuration (AWS IoT Broker - same as Lavli Master Node)
// ============================================================================

#define MQTT_BROKER "b-f3e16066-c8b5-48a8-81b0-bc3347afef80-1.mq.us-east-1.amazonaws.com"
#define MQTT_PORT 8883  // TLS/SSL port
#define MQTT_USERNAME "admin"
#define MQTT_PASSWORD "lavlidevbroker!321"

// MQTT topic format: /lavli/{hardwareId}/{command}
// Commands: dry, wash, stop

// ============================================================================
// Machine Status Enumeration
// ============================================================================

enum MachineStatus {
    MACHINE_IDLE = 0,
    MACHINE_WASHING = 1,
    MACHINE_DRYING = 2
};

// Convert MachineStatus enum to string for API
inline const char* machineStatusToString(MachineStatus status) {
    switch (status) {
        case MACHINE_IDLE:    return "IDLE";
        case MACHINE_WASHING: return "WASHING";
        case MACHINE_DRYING:  return "DRYING";
        default:              return "IDLE";
    }
}

#endif // CONFIG_H
