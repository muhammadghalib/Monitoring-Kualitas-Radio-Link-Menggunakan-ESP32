// =========================== INCLUDE FILES ===========================
// ======================== LIBRARY INCLUSIONS =========================

#include <WiFi.h>
#include <WiFiUdp.h>
#include <SNMP_Agent.h>
#include "thingPropertiesVorlinkvueDeveloper.h"



// =================== DEFINITIONS AND GLOBAL VARIABLES ================
// ====================== LED CONTROL VARIABLES ========================

#define LED_PIN 2

bool blinkLed = false;
bool ledSolidOn = false;

// ====================== Network Configuration ========================

IPAddress localIP (192, 168, 0, 111);
IPAddress gateway (192, 168, 0, 1);

// ============ SNMP CONFIGURATION AND CALLBACK DEFINITIONS =============

WiFiUDP udp;
SNMPAgent snmp("public", "private");

int rssiValue = 0;
int snrValue = 0;
char accessPointName[32] = "";
char* accessPointNamePtr = accessPointName;
char distanceValue[32] = "";
char* distanceValuePtr = distanceValue;
int frequencyValue = 0;
int transmitterPowerValue = 0;
int antennaGainValue = 0;

ValueCallback* rssiOID;
ValueCallback* snrOID;
ValueCallback* accessPointNameOID;
ValueCallback* distanceOID;
ValueCallback* frequencyOID;
ValueCallback* transmitterPowerOID;
ValueCallback* antennaGainOID;

unsigned long previousMillis = 0;
const unsigned long interval = 500;

bool ipPrinted = false;
IPAddress currentIPAddress;



// ========================== CORE FUNCTIONS ===========================
// ========================== SETUP FUNCTION ===========================

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();

  pinMode(LED_PIN, OUTPUT);
  blinkLed = false;
  ledSolidOn = true;

  if (!WiFi.config(localIP, gateway)) {
    Serial.println("Static IP Failed to configure");
  }

  WiFi.begin(SSID, PASS);

  setupArduinoIOTCloud();
  setupSNMP();

  xTaskCreatePinnedToCore(taskBlinkLED, "taskBlinkLED", 1024, NULL, 1, NULL, 0);
}

// ============================ LOOP FUNCTION ===========================

void loop() {
  snmp.loop();
  ArduinoCloud.update();
  printIPAddress();
  startSNMPAgent();
}

// ============================ UTILITY FUNCTION ===========================

void printIPAddress() {
  if (!ipPrinted) {
    currentIPAddress = WiFi.localIP();
    if (currentIPAddress.toString() != "0.0.0.0") {
      Serial.print("IP Address : ");
      Serial.println(currentIPAddress);
      ipPrinted = true;
    }
  }
}

void setupArduinoIOTCloud() {
  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
}

void setupSNMP() {
  snmp.setUDP(&udp);
  snmp.begin();

  rssiOID = snmp.addIntegerHandler(".1.3.6.1.4.1.17713.21.1.2.3.0", &rssiValue);
  snrOID = snmp.addIntegerHandler(".1.3.6.1.4.1.17713.21.1.2.18.0", &snrValue);
  accessPointNameOID = snmp.addReadWriteStringHandler(".1.3.6.1.4.1.17713.21.1.2.8.0", &accessPointNamePtr, sizeof(accessPointName), false);
  distanceOID = snmp.addReadWriteStringHandler(".1.3.6.1.4.1.17713.21.1.2.12.0", &distanceValuePtr, sizeof(distanceValue), false);
  frequencyOID = snmp.addIntegerHandler(".1.3.6.1.4.1.17713.21.1.2.20.1.4.1", &frequencyValue);
  transmitterPowerOID = snmp.addIntegerHandler(".1.3.6.1.4.1.17713.21.3.8.2.6.0", &transmitterPowerValue);
  antennaGainOID = snmp.addIntegerHandler(".1.3.6.1.4.1.17713.21.3.8.2.7.0", &antennaGainValue);

  snmp.sortHandlers();
}

// ========================== FREE RTOS FUNCTION ========================

void taskBlinkLED(void *parameter) {
  while (true) {
    if (blinkLed) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      vTaskDelay(pdMS_TO_TICKS(100));
    } else {
      digitalWrite(LED_PIN, ledSolidOn ? HIGH : LOW);
      vTaskDelay(pdMS_TO_TICKS(200));
    }
  }
}

// =========================== Arduino IOT Cloud FUNCTION =========================

void onKoneksiSangatBaikChange()  {
  if (koneksiSangatBaik) {
    koneksiBaik = false;
    koneksiCukup = false;
    koneksiBuruk = false;
    koneksiTerputus = false;
  }
}

void onKoneksiBaikChange()  {
  if (koneksiBaik) {
    koneksiSangatBaik = false;
    koneksiCukup = false;
    koneksiBuruk = false;
    koneksiTerputus = false;
  }
}

void onKoneksiCukupChange()  {
  if (koneksiCukup) {
    koneksiSangatBaik = false;
    koneksiBaik = false;
    koneksiBuruk = false;
    koneksiTerputus = false;
  }
}

void onKoneksiBurukChange()  {
  if (koneksiBuruk) {
    koneksiSangatBaik = false;
    koneksiBaik = false;
    koneksiCukup = false;
    koneksiTerputus = false;
  }
}

void onKoneksiTerputusChange()  {
  if (koneksiTerputus) {
    koneksiSangatBaik = false;
    koneksiBaik = false;
    koneksiCukup = false;
    koneksiBuruk = false;
  }
}

// =========================== SNMP Agent FUNCTION =========================

void startSNMPAgent() {
  if (!ArduinoCloud.connected()) return;

  blinkLed = true;
  ledSolidOn = false;

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    if (koneksiSangatBaik) {
      Serial.println("\n[Arduino IOT Cloud] Koneksi Sangat Baik");

      rssiValue = random(-69, 0);
      snrValue = random(25, 35);
      strncpy(accessPointName, "DVOR-AP", sizeof(accessPointName));
      strncpy(distanceValue, "5.696", sizeof(distanceValue));
      frequencyValue = 5080;
      transmitterPowerValue = 28;
      antennaGainValue = 24;
    }
    else if (koneksiBaik) {
      Serial.println("\n[Arduino IOT Cloud] Koneksi Baik");

      rssiValue = random(-85, -69);
      snrValue = random(20, 25);
      strncpy(accessPointName, "DVOR-AP", sizeof(accessPointName));
      strncpy(distanceValue, "5.696", sizeof(distanceValue));
      frequencyValue = 5080;
      transmitterPowerValue = 28;
      antennaGainValue = 24;
    }
    else if (koneksiCukup) {
      Serial.println("\n[Arduino IOT Cloud] Koneksi Cukup");

      rssiValue = random(-100, -85);
      snrValue = random(10, 15);
      strncpy(accessPointName, "DVOR-AP", sizeof(accessPointName));
      strncpy(distanceValue, "5.696", sizeof(distanceValue));
      frequencyValue = 5080;
      transmitterPowerValue = 28;
      antennaGainValue = 24;
    }
    else if (koneksiBuruk) {
      Serial.println("\n[Arduino IOT Cloud] Koneksi Buruk");

      rssiValue = random(-150, -100);
      snrValue = random(5, 10);
      strncpy(accessPointName, "DVOR-AP", sizeof(accessPointName));
      strncpy(distanceValue, "5.696", sizeof(distanceValue));
      frequencyValue = 5080;
      transmitterPowerValue = 28;
      antennaGainValue = 24;
    }
    else if (koneksiTerputus) {
      Serial.println("\n[Arduino IOT Cloud] Koneksi Terputus");

      rssiValue = 0;
      snrValue = 0;
      strncpy(accessPointName, "Scanning...", sizeof(accessPointName));
      strncpy(distanceValue, "0", sizeof(distanceValue));
      frequencyValue = 5080;
      transmitterPowerValue = 28;
      antennaGainValue = 24;
    }
    else {
      Serial.println("\n[Arduino IOT Cloud] Tidak ada opsi yang dipilih");

      rssiValue = 0;
      snrValue = 0;
      strncpy(accessPointName, "", sizeof(accessPointName));
      strncpy(distanceValue, "", sizeof(distanceValue));
      frequencyValue = 0;
      transmitterPowerValue = 0;
      antennaGainValue = 0;
    }

    Serial.print(F("\n[SNMP] RSSI               : "));  Serial.print(rssiValue);              Serial.println(F(" dBm"));
    Serial.print(F("[SNMP] SNR                : "));    Serial.print(snrValue);               Serial.println(F(" dB"));
    Serial.print(F("[SNMP] Access Point       : "));    Serial.println(accessPointName);
    Serial.print(F("[SNMP] Distance           : "));    Serial.print(distanceValue);          Serial.println(F(" Km"));
    Serial.print(F("[SNMP] Frequency          : "));    Serial.print(frequencyValue);         Serial.println(F(" Mhz"));
    Serial.print(F("[SNMP] Transmitter Power  : "));    Serial.print(transmitterPowerValue);  Serial.println(F(" dBm"));
    Serial.print(F("[SNMP] Antenna Gain       : "));    Serial.print(antennaGainValue);       Serial.println(F(" dBi"));
  }
}
