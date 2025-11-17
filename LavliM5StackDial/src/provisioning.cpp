#include "provisioning.h"
#include "mac_utils.h"

// Global state variables
ConnectionState currentConnectionState = STATE_DISCONNECTED;
bool bleActive = false;
bool wifiConnected = false;
String deviceSSID = "";
String devicePassword = "";

// BLE objects
NimBLEServer* bleServer = nullptr;
NimBLEAdvertising* bleAdvertising = nullptr;
NimBLECharacteristic* statusCharacteristic = nullptr;
NimBLECharacteristic* macCharacteristic = nullptr;

// WiFi Manager
WiFiManager wifiManager;

// Internal flags
volatile bool connectRequested = false;

bool initializeBLE() {
  Serial.println("[BLE] Initializing BLE provisioning...");
  
  NimBLEDevice::init("Lavli-Dial");
  bleServer = NimBLEDevice::createServer();
  
  if (!bleServer) {
    Serial.println("[BLE] Failed to create BLE server");
    return false;
  }
  
  // Create service
  NimBLEService* service = bleServer->createService(SERVICE_UUID);
  if (!service) {
    Serial.println("[BLE] Failed to create BLE service");
    return false;
  }
  
  // SSID characteristic (write)
  NimBLECharacteristic* ssidChar = service->createCharacteristic(
    SSID_CHAR_UUID, NIMBLE_PROPERTY::WRITE
  );
  ssidChar->setCallbacks(new SSIDWriteCallback());
  
  // Password characteristic (write)
  NimBLECharacteristic* passChar = service->createCharacteristic(
    PASS_CHAR_UUID, NIMBLE_PROPERTY::WRITE
  );
  passChar->setCallbacks(new PassWriteCallback());
  
  // Control characteristic (write)
  NimBLECharacteristic* controlChar = service->createCharacteristic(
    CONTROL_CHAR_UUID, NIMBLE_PROPERTY::WRITE
  );
  controlChar->setCallbacks(new ControlWriteCallback());
  
  // Status characteristic (notify)
  statusCharacteristic = service->createCharacteristic(
    STATUS_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY
  );

  // MAC Address characteristic (read-only)
  macCharacteristic = service->createCharacteristic(
    MAC_CHAR_UUID, NIMBLE_PROPERTY::READ
  );

  // Set MAC address value
  String macAddress = getMACAddress();
  macCharacteristic->setValue(macAddress.c_str());
  Serial.printf("[BLE] MAC Address exposed via BLE: %s\n", macAddress.c_str());

  // Start service
  service->start();
  
  // Start advertising
  bleAdvertising = NimBLEDevice::getAdvertising();
  bleAdvertising->addServiceUUID(SERVICE_UUID);
  bleAdvertising->setScanResponse(true);
  bleAdvertising->setMinPreferred(0x06);
  bleAdvertising->setMaxPreferred(0x12);
  bleAdvertising->start();
  
  bleActive = true;
  currentConnectionState = STATE_BLE_ACTIVE;
  
  Serial.println("[BLE] BLE provisioning active - waiting for credentials");
  notifyBLEStatus("BLE Ready - Send WiFi credentials");
  
  return true;
}

void shutdownBLE() {
  if (!bleActive) return;
  
  Serial.println("[BLE] Shutting down BLE...");
  bleActive = false;
  
  if (bleAdvertising) {
    bleAdvertising->stop();
  }
  
  NimBLEDevice::deinit(true);
  notifyBLEStatus("BLE shutdown");
  
  Serial.println("[BLE] BLE shutdown complete");
}

bool setupWiFi() {
  Serial.println("[WIFI] Setting up WiFi...");
  
  if (USE_WIFI_MANAGER && deviceSSID.isEmpty()) {
    Serial.println("[WIFI] Using WiFiManager provisioning");
    
    wifiManager.setAPCallback([](WiFiManager *myWiFiManager) {
      Serial.println("[WIFI] Entered WiFiManager config mode");
      Serial.println("[WIFI] AP Name: " + String(AP_NAME));
      Serial.println("[WIFI] IP: " + WiFi.softAPIP().toString());
      currentConnectionState = STATE_WIFI_CONNECTING;
    });
    
    if (!wifiManager.autoConnect(AP_NAME)) {
      Serial.println("[WIFI] WiFiManager connection failed");
      currentConnectionState = STATE_WIFI_FAILED;
      return false;
    }
  } else if (!deviceSSID.isEmpty()) {
    Serial.println("[WIFI] Using BLE-provided credentials");
    return connectWiFiBlocking(deviceSSID, devicePassword);
  } else {
    Serial.println("[WIFI] No WiFi credentials available");
    return false;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] WiFi connected successfully!");
    Serial.print("[WIFI] IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("[WIFI] Signal strength (RSSI): ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    
    wifiConnected = true;
    currentConnectionState = STATE_WIFI_CONNECTED;
    notifyBLEStatus(("WiFi connected: " + WiFi.localIP().toString()).c_str());
    
    // Shutdown BLE after successful WiFi connection
    if (bleActive) {
      delay(2000); // Give time for final BLE notification
      shutdownBLE();
    }
    
    return true;
  }
  
  return false;
}

bool connectWiFiBlocking(const String& ssid, const String& pass, uint32_t timeoutMs) {
  if (ssid.isEmpty()) {
    notifyBLEStatus("SSID empty");
    return false;
  }
  
  currentConnectionState = STATE_WIFI_CONNECTING;
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);
  
  notifyBLEStatus(("Connecting to: " + ssid).c_str());
  WiFi.begin(ssid.c_str(), pass.isEmpty() ? nullptr : pass.c_str());
  
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    String msg = "WiFi connected: " + WiFi.localIP().toString();
    notifyBLEStatus(msg.c_str());
    wifiConnected = true;
    currentConnectionState = STATE_WIFI_CONNECTED;
    return true;
  } else {
    notifyBLEStatus("WiFi connection failed");
    currentConnectionState = STATE_WIFI_FAILED;
    return false;
  }
}

void notifyBLEStatus(const char* msg) {
  Serial.printf("[BLE] Status: %s\n", msg);
  if (statusCharacteristic && bleActive) {
    statusCharacteristic->setValue((uint8_t*)msg, strlen(msg));
    statusCharacteristic->notify();
  }
}

void updateConnectionStatus() {
  static unsigned long lastCheck = 0;
  if (millis() - lastCheck < 5000) return; // Check every 5 seconds
  lastCheck = millis();
  
  if (wifiConnected && WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] WiFi connection lost");
    wifiConnected = false;
    currentConnectionState = STATE_DISCONNECTED;
  } else if (!wifiConnected && WiFi.status() == WL_CONNECTED) {
    Serial.println("[WIFI] WiFi reconnected");
    wifiConnected = true;
    currentConnectionState = STATE_WIFI_CONNECTED;
  }
}

void processProvisioning() {
  if (connectRequested && !deviceSSID.isEmpty()) {
    connectRequested = false;
    Serial.println("[BLE] Processing connection request...");
    setupWiFi();
  }
  
  updateConnectionStatus();
}

// BLE Callback implementations
void SSIDWriteCallback::onWrite(NimBLECharacteristic* c) {
  std::string value = c->getValue();
  deviceSSID = String(value.c_str());
  notifyBLEStatus(("SSID set: " + deviceSSID).c_str());
  Serial.printf("[BLE] SSID received: %s\n", deviceSSID.c_str());
}

void PassWriteCallback::onWrite(NimBLECharacteristic* c) {
  std::string value = c->getValue();
  devicePassword = String(value.c_str());
  notifyBLEStatus("Password received");
  Serial.println("[BLE] Password received (hidden for security)");
}

void ControlWriteCallback::onWrite(NimBLECharacteristic* c) {
  std::string value = c->getValue();
  String command = String(value.c_str());
  command.toUpperCase();
  
  Serial.printf("[BLE] Control command: %s\n", command.c_str());
  
  if (command == "CONNECT") {
    if (!deviceSSID.isEmpty()) {
      notifyBLEStatus("Starting WiFi connection...");
      connectRequested = true;
    } else {
      notifyBLEStatus("Error: SSID not set");
    }
  } else {
    notifyBLEStatus("Unknown command");
  }
}