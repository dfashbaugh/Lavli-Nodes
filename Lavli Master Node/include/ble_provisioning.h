#ifndef BLE_PROVISIONING_H
#define BLE_PROVISIONING_H

#include <Arduino.h>

// BLE Provisioning Status
enum BLEProvisioningStatus {
  BLE_PROV_IDLE,
  BLE_PROV_ADVERTISING,
  BLE_PROV_CREDENTIALS_RECEIVED,
  BLE_PROV_CONNECTING,
  BLE_PROV_WIFI_CONNECTED,
  BLE_PROV_WIFI_FAILED,
  BLE_PROV_SHUTDOWN
};

// BLE Provisioning API
class BLEProvisioning {
public:
  // Initialize and start BLE provisioning
  static void begin(const char* deviceName = "Lavli WiFi Provisioner");

  // Process BLE provisioning state machine (call in loop)
  static void process();

  // Check if WiFi is connected via BLE provisioning
  static bool isWiFiConnected();

  // Get current provisioning status
  static BLEProvisioningStatus getStatus();

  // Shutdown BLE (called after successful WiFi connection)
  static void shutdown();

  // Notify status to BLE client (if connected)
  static void notifyStatus(const char* message);

private:
  static BLEProvisioningStatus status;
  static bool connectRequested;
  static String ssid;
  static String password;
};

#endif // BLE_PROVISIONING_H
