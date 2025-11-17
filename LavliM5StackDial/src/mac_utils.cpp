#include "mac_utils.h"
#include <WiFi.h>

String getMACAddress() {
    uint8_t mac[6];
    WiFi.macAddress(mac);

    char macStr[13]; // 12 hex chars + null terminator
    sprintf(macStr, "%02X%02X%02X%02X%02X%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return String(macStr);
}
