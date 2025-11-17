#include "api_client.h"
#include "mac_utils.h"
#include <HTTPClient.h>
#include <WiFi.h>

// Current machine state
static MachineStatus currentStatus = MACHINE_IDLE;
static String statusMessage = "Normal";
static unsigned long lastCheckinTime = 0;
static String hardwareId = "";

void initializeAPIClient() {
    Serial.println("Initializing API Client...");

    // Get MAC address once and store it
    hardwareId = getMACAddress();

    Serial.print("Hardware ID: ");
    Serial.println(hardwareId);

#if ENABLE_API_CHECKIN
    String fullUrl = String(BASE_URL) + String(API_CHECKIN_ENDPOINT);
    Serial.print("API Check-in URL: ");
    Serial.println(fullUrl);
    Serial.print("Check-in interval: ");
    Serial.print(API_CHECKIN_INTERVAL_MS);
    Serial.println("ms");
#else
    Serial.println("API Check-in is DISABLED (ENABLE_API_CHECKIN = false)");
#endif
}

void setMachineStatus(MachineStatus status) {
    currentStatus = status;
}

void setMachineStatusMessage(const char* message) {
    statusMessage = String(message);
}

unsigned long getLastCheckinTime() {
    return lastCheckinTime;
}

bool isAPICheckinEnabled() {
#if ENABLE_API_CHECKIN
    return true;
#else
    return false;
#endif
}

void processAPICheckin() {
#if ENABLE_API_CHECKIN
    // Check if WiFi is connected
    if (WiFi.status() != WL_CONNECTED) {
        return;
    }

    // Check if it's time for check-in
    unsigned long now = millis();
    if (now - lastCheckinTime < API_CHECKIN_INTERVAL_MS) {
        return;
    }

    lastCheckinTime = now;

    // Prepare HTTP client
    HTTPClient http;
    String fullUrl = String(BASE_URL) + String(API_CHECKIN_ENDPOINT);

    http.begin(fullUrl);
    http.addHeader("Content-Type", "application/json");

    // Build JSON body manually (simple enough to not need ArduinoJson)
    String jsonBody = "{";
    jsonBody += "\"hardwareId\":\"" + hardwareId + "\",";
    jsonBody += "\"machineStatus\":\"" + String(machineStatusToString(currentStatus)) + "\",";
    jsonBody += "\"machineStatusMessage\":\"" + statusMessage + "\"";
    jsonBody += "}";

    // Send POST request
    int httpResponseCode = http.POST(jsonBody);

    if (httpResponseCode > 0) {
        Serial.print("API Check-in successful, response code: ");
        Serial.println(httpResponseCode);

        if (httpResponseCode == 200) {
            String response = http.getString();
            Serial.print("Response: ");
            Serial.println(response);
        }
    } else {
        Serial.print("API Check-in failed, error: ");
        Serial.println(http.errorToString(httpResponseCode));
    }

    http.end();
#endif
}
