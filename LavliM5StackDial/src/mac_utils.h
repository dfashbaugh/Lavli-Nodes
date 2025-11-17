#ifndef MAC_UTILS_H
#define MAC_UTILS_H

#include <Arduino.h>

/**
 * Get the device's MAC address as a string
 * Returns MAC address without colons (e.g., "AABBCCDDEEFF")
 * This is used as the hardware ID across MQTT, API, and BLE
 */
String getMACAddress();

#endif // MAC_UTILS_H
