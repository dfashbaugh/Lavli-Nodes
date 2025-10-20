#include "ble_provisioning.h"
#include <WiFi.h>
#include <NimBLEDevice.h>

// ----------- BLE Service/Characteristic UUIDs (custom) -------------
static const char* SERVICE_UUID       = "7b1e0001-0d7a-4c80-9f31-7f9c18a2b001";
static const char* SSID_CHAR_UUID     = "7b1e0002-0d7a-4c80-9f31-7f9c18a2b001"; // write
static const char* PASS_CHAR_UUID     = "7b1e0003-0d7a-4c80-9f31-7f9c18a2b001"; // write (no read)
static const char* CONTROL_CHAR_UUID  = "7b1e0004-0d7a-4c80-9f31-7f9c18a2b001"; // write "CONNECT"
static const char* STATUS_CHAR_UUID   = "7b1e0005-0d7a-4c80-9f31-7f9c18a2b001"; // notify

// ----------- Static member initialization -------------
BLEProvisioningStatus BLEProvisioning::status = BLE_PROV_IDLE;
bool BLEProvisioning::connectRequested = false;
String BLEProvisioning::ssid = "";
String BLEProvisioning::password = "";

// ----------- BLE Globals -------------
static NimBLEServer*         g_server        = nullptr;
static NimBLEAdvertising*    g_advertising   = nullptr;
static NimBLECharacteristic* g_statusChar    = nullptr;
static bool                  g_bleActive     = false;

// ----------- BLE Callbacks -------------
class SSIDWriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& /*conn*/) override {
    std::string v = c->getValue();
    BLEProvisioning::ssid = String(v.c_str());
    BLEProvisioning::notifyStatus(("SSID set: " + BLEProvisioning::ssid).c_str());
    Serial.println("[BLE] SSID set: " + BLEProvisioning::ssid);
  }
};

class PassWriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& /*conn*/) override {
    std::string v = c->getValue();
    BLEProvisioning::password = String(v.c_str());
    BLEProvisioning::notifyStatus("Password set");
    Serial.println("[BLE] Password set");
  }
};

class ControlWriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& /*conn*/) override {
    std::string v = c->getValue();
    String cmd = String(v.c_str());
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "CONNECT") {
      BLEProvisioning::notifyStatus("CONNECT requested");
      BLEProvisioning::connectRequested = true;
      Serial.println("[BLE] CONNECT command received");
    } else if (cmd == "CLEAR") {
      BLEProvisioning::ssid = "";
      BLEProvisioning::password = "";
      BLEProvisioning::notifyStatus("Credentials cleared");
      Serial.println("[BLE] Credentials cleared");
    } else {
      BLEProvisioning::notifyStatus("Unknown command");
      Serial.println("[BLE] Unknown command: " + cmd);
    }
  }
};

// ----------- BLE Provisioning Implementation -------------
void BLEProvisioning::begin(const char* deviceName) {
  Serial.println("[BLE] Initializing BLE provisioning...");

  NimBLEDevice::init(deviceName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(false, false, true); // no bonding/MITM

  g_server = NimBLEDevice::createServer();
  NimBLEService* svc = g_server->createService(SERVICE_UUID);

  // SSID Characteristic
  auto* ssidChar = svc->createCharacteristic(
    SSID_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  ssidChar->setCallbacks(new SSIDWriteCallback());

  // Password Characteristic
  auto* passChar = svc->createCharacteristic(
    PASS_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  passChar->setCallbacks(new PassWriteCallback());

  // Control Characteristic
  auto* ctrlChar = svc->createCharacteristic(
    CONTROL_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  ctrlChar->setCallbacks(new ControlWriteCallback());

  // Status Characteristic (for notifications)
  g_statusChar = svc->createCharacteristic(
    STATUS_CHAR_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  svc->start();

  g_advertising = NimBLEDevice::getAdvertising();
  g_advertising->addServiceUUID(SERVICE_UUID);

  NimBLEAdvertisementData scanResp;
  scanResp.setName(deviceName);
  g_advertising->setScanResponseData(scanResp);

  g_advertising->start();

  g_bleActive = true;
  status = BLE_PROV_ADVERTISING;

  Serial.println("[BLE] BLE advertising started");
  Serial.println("[BLE] Waiting for SSID/PASS, then send 'CONNECT' to CONTROL characteristic");
  Serial.println("[BLE] CONTROL supports: CONNECT, CLEAR");

  notifyStatus("BLE advertising - ready for credentials");
}

void BLEProvisioning::process() {
  if (!g_bleActive) {
    return;
  }

  // Handle WiFi connection request
  if (connectRequested) {
    connectRequested = false;

    if (ssid.length() == 0) {
      notifyStatus("No SSID set");
      Serial.println("[BLE] Cannot connect - no SSID");
      return;
    }

    status = BLE_PROV_CONNECTING;
    notifyStatus(("Connecting to: " + ssid).c_str());
    Serial.println("[WIFI] Attempting connection to: " + ssid);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true, true);
    delay(200);

    WiFi.begin(ssid.c_str(), password.isEmpty() ? nullptr : password.c_str());

    uint32_t start = millis();
    uint32_t timeout = 20000; // 20 second timeout

    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
      delay(200);
      Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      status = BLE_PROV_WIFI_CONNECTED;
      String msg = "WiFi connected: " + WiFi.localIP().toString();
      notifyStatus(msg.c_str());
      Serial.println("[WIFI] " + msg);

      // Automatically shutdown BLE after successful connection
      delay(1000); // Give time for notification to be sent
      shutdown();
    } else {
      status = BLE_PROV_WIFI_FAILED;
      notifyStatus("WiFi connection failed");
      Serial.println("[WIFI] Connection failed");
      Serial.println("[BLE] Retry with different credentials or send CONNECT again");
    }
  }

  // Monitor WiFi connection status
  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (WiFi.getMode() == WIFI_MODE_STA && status == BLE_PROV_WIFI_CONNECTED) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WIFI] Disconnected, attempting reconnect...");
        WiFi.reconnect();
      }
    }
  }
}

bool BLEProvisioning::isWiFiConnected() {
  return (status == BLE_PROV_WIFI_CONNECTED && WiFi.status() == WL_CONNECTED);
}

BLEProvisioningStatus BLEProvisioning::getStatus() {
  return status;
}

void BLEProvisioning::shutdown() {
  if (!g_bleActive) {
    return;
  }

  g_bleActive = false;
  status = BLE_PROV_SHUTDOWN;

  Serial.println("[BLE] Shutting down BLE...");
  notifyStatus("BLE shutting down");

  if (g_advertising) {
    g_advertising->stop();
  }

  NimBLEDevice::deinit(true); // fully deinit BLE stack

  Serial.println("[BLE] BLE shutdown complete");
}

void BLEProvisioning::notifyStatus(const char* msg) {
  if (g_statusChar && g_bleActive) {
    g_statusChar->setValue((uint8_t*)msg, strlen(msg));
    g_statusChar->notify();
  }
}
