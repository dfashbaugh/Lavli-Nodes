#include <Arduino.h>
#include <WiFi.h>
#include <NimBLEDevice.h>

// ----------- BLE Service/Characteristic UUIDs (custom) -------------
static const char* SERVICE_UUID       = "7b1e0001-0d7a-4c80-9f31-7f9c18a2b001";
static const char* SSID_CHAR_UUID     = "7b1e0002-0d7a-4c80-9f31-7f9c18a2b001"; // write
static const char* PASS_CHAR_UUID     = "7b1e0003-0d7a-4c80-9f31-7f9c18a2b001"; // write (no read)
static const char* CONTROL_CHAR_UUID  = "7b1e0004-0d7a-4c80-9f31-7f9c18a2b001"; // write "CONNECT"
static const char* STATUS_CHAR_UUID   = "7b1e0005-0d7a-4c80-9f31-7f9c18a2b001"; // notify

// ----------- Globals -------------
String g_ssid;
String g_pass;

NimBLEServer*         g_server        = nullptr;
NimBLEAdvertising*    g_advertising   = nullptr;
NimBLECharacteristic* gStatusChar     = nullptr;

volatile bool g_connectRequested = false;
volatile bool g_bleActive        = true;

const int LED = 2;

// ----------- Helpers -------------
void setLed(bool on) {
  pinMode(LED, OUTPUT);
  digitalWrite(LED, on ? HIGH : LOW);
}

void notifyStatus(const char* msg) {
  Serial.println(msg);
  if (gStatusChar && g_bleActive) {
    gStatusChar->setValue((uint8_t*)msg, strlen(msg));
    gStatusChar->notify();
  }
}

bool connectWiFiBlocking(const String& ssid, const String& pass, uint32_t timeoutMs = 20000) {
  if (ssid.isEmpty()) {
    notifyStatus("SSID empty");
    return false;
  }
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);

  notifyStatus(("WiFi begin: " + ssid).c_str());
  WiFi.begin(ssid.c_str(), pass.isEmpty() ? nullptr : pass.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
    Serial.print(".");
    setLed(((millis() / 200) % 2) == 0);
  }
  setLed(false);

  if (WiFi.status() == WL_CONNECTED) {
    String ok = "WiFi connected: " + WiFi.localIP().toString();
    notifyStatus(ok.c_str());
    return true;
  } else {
    notifyStatus("WiFi failed");
    return false;
  }
}

void shutdownBLE() {
  if (!g_bleActive) return;
  g_bleActive = false;
  notifyStatus("Shutting BLE down");
  if (g_advertising) g_advertising->stop();
  // do NOT call g_server->stop(); it doesn't exist in NimBLE
  NimBLEDevice::deinit(true); // fully deinit BLE stack
  notifyStatus("BLE off");
}

// ----------- BLE Callbacks -------------
// Note: two-parameter onWrite signature in NimBLE 2.x
class SSIDWriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& /*conn*/) override {
    std::string v = c->getValue();
    g_ssid = String(v.c_str());
    notifyStatus(("SSID set: " + g_ssid).c_str());
  }
};

class PassWriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& /*conn*/) override {
    std::string v = c->getValue();
    g_pass = String(v.c_str());
    notifyStatus("Password set");
  }
};

class ControlWriteCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& /*conn*/) override {
    std::string v = c->getValue();
    String cmd = String(v.c_str());
    cmd.trim();
    cmd.toUpperCase();
    if (cmd == "CONNECT") {
      notifyStatus("CONNECT requested");
      g_connectRequested = true;
    } else if (cmd == "CLEAR") {
      g_ssid = "";
      g_pass = "";
      notifyStatus("Credentials cleared");
    } else {
      notifyStatus("Unknown command");
    }
  }
};

// ----------- Setup BLE GATT -------------
void startBLE() {
  NimBLEDevice::init("ESP32 WiFi Provisioner");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  NimBLEDevice::setSecurityAuth(false, false, true); // no bonding/MITM

  g_server = NimBLEDevice::createServer();
  NimBLEService* svc = g_server->createService(SERVICE_UUID);

  auto* ssidChar = svc->createCharacteristic(
    SSID_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  ssidChar->setCallbacks(new SSIDWriteCallback());

  auto* passChar = svc->createCharacteristic(
    PASS_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  passChar->setCallbacks(new PassWriteCallback());

  auto* ctrlChar = svc->createCharacteristic(
    CONTROL_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  ctrlChar->setCallbacks(new ControlWriteCallback());

  gStatusChar = svc->createCharacteristic(
    STATUS_CHAR_UUID,
    NIMBLE_PROPERTY::NOTIFY
  );

  svc->start();

  g_advertising = NimBLEDevice::getAdvertising();
  g_advertising->addServiceUUID(SERVICE_UUID);
  // If you want a scan response, compose data and call setScanResponseData(...).

  NimBLEAdvertisementData scanResp;
  scanResp.setName("Lavli WiFi Provisioner");
  g_advertising->setScanResponseData(scanResp);

  g_advertising->start();

  notifyStatus("BLE advertising");
}

// ----------- Arduino Setup/Loop -------------
void setup() {
  Serial.begin(115200);
  delay(200);
  setLed(false);

  startBLE();

  notifyStatus("Write SSID/PASS, then write 'CONNECT' to CONTROL");
  notifyStatus("CONTROL supports: CONNECT, CLEAR");
}

void loop() {
  if (g_connectRequested) {
    g_connectRequested = false;

    if (g_ssid.length() == 0) {
      notifyStatus("No SSID set");
      return;
    }

    bool ok = connectWiFiBlocking(g_ssid, g_pass);
    if (ok) {
      shutdownBLE();
      // Now do your main online work...
    } else {
      notifyStatus("Retry credentials or send CONNECT again");
    }
  }

  static uint32_t lastCheck = 0;
  if (millis() - lastCheck > 5000) {
    lastCheck = millis();
    if (WiFi.getMode() == WIFI_MODE_STA) {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] disconnected, retrying...");
        WiFi.reconnect();
      }
    }
  }

  delay(10);
}
