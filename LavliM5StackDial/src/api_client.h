#ifndef API_CLIENT_H
#define API_CLIENT_H

#include <Arduino.h>
#include "config.h"

/**
 * Initialize API client
 * Must be called after WiFi is connected
 */
void initializeAPIClient();

/**
 * Set the current machine status
 * This will be included in the next API check-in
 */
void setMachineStatus(MachineStatus status);

/**
 * Set the machine status message
 * This will be included in the next API check-in
 */
void setMachineStatusMessage(const char* message);

/**
 * Process API check-in
 * Non-blocking - uses timer to send check-in every API_CHECKIN_INTERVAL_MS
 * Call this frequently in main loop
 */
void processAPICheckin();

/**
 * Get the last check-in timestamp
 */
unsigned long getLastCheckinTime();

/**
 * Check if API check-in is enabled
 */
bool isAPICheckinEnabled();

#endif // API_CLIENT_H
