#ifndef PROVISIONING_H
#define PROVISIONING_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <NimBLEDevice.h>

// Provisioning configuration
#define USE_BLE_PROVISIONING true
#define USE_WIFI_MANAGER false
#define AP_NAME "Lavli-Dial-Setup"

// BLE Service/Characteristic UUIDs (matching BLE Provisioner)
#define SERVICE_UUID      "7b1e0001-0d7a-4c80-9f31-7f9c18a2b001"
#define SSID_CHAR_UUID    "7b1e0002-0d7a-4c80-9f31-7f9c18a2b001"
#define PASS_CHAR_UUID    "7b1e0003-0d7a-4c80-9f31-7f9c18a2b001"
#define CONTROL_CHAR_UUID "7b1e0004-0d7a-4c80-9f31-7f9c18a2b001"
#define STATUS_CHAR_UUID  "7b1e0005-0d7a-4c80-9f31-7f9c18a2b001"
#define MAC_CHAR_UUID     "7b1e0006-0d7a-4c80-9f31-7f9c18a2b001"

// Connection states
enum ConnectionState {
  STATE_DISCONNECTED = 0,
  STATE_BLE_ACTIVE = 1,
  STATE_WIFI_CONNECTING = 2,
  STATE_WIFI_CONNECTED = 3,
  STATE_WIFI_FAILED = 4
};

// Global state variables
extern ConnectionState currentConnectionState;
extern bool bleActive;
extern bool wifiConnected;
extern String deviceSSID;
extern String devicePassword;

// BLE objects
extern NimBLEServer* bleServer;
extern NimBLEAdvertising* bleAdvertising;
extern NimBLECharacteristic* statusCharacteristic;
extern NimBLECharacteristic* macCharacteristic;

// WiFi Manager
extern WiFiManager wifiManager;

// Function declarations
bool initializeBLE();
void shutdownBLE();
bool setupWiFi();
void updateConnectionStatus();
void processProvisioning();
bool connectWiFiBlocking(const String& ssid, const String& pass, uint32_t timeoutMs = 20000);
void notifyBLEStatus(const char* msg);

// BLE Callback classes
class SSIDWriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override;
};

class PassWriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override;
};

class ControlWriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c) override;
};

#endif // PROVISIONING_H