// =========================== INCLUDE FILES ===========================
// ======================== LIBRARY INCLUSIONS =========================

#include <ETH.h>
#include <ESP32Ping.h>
#include <WiFiUdp.h>
#include <Arduino_SNMP_Manager.h>
#include <HardwareSerial.h>
#include <esp_system.h>



// =================== DEFINITIONS AND GLOBAL VARIABLES ================
// ====================== LED CONTROL VARIABLES ========================

#define LED_PIN 2

bool blinkLed = false;
bool ledSolidOn = false;

// ======================= W5500 PIN DEFINITIONS =======================

#define ETH_IRQ      13
#define ETH_CS       14
#define ETH_SPI_SCK  27
#define ETH_SPI_MOSI 26
#define ETH_SPI_MISO 25
#define ETH_RST      33

// =================== NETWORK ADDRESS CONFIGURATION ===================

const IPAddress localIP   (192, 168, 0, 250);
const IPAddress gateway   (192, 168, 0, 1);
const IPAddress subnet    (255, 255, 255, 0);

// ==================== RADIO LINK TOWER IP ADDRESS ====================

const IPAddress radioLinkTower  (192, 168, 0, 111);

// ==================== RADIO LINK DATA CONTAINER ======================

int rssiValue = 0;
int snrValue = 0;
char accessPointBuffer[32], *accessPointValue = accessPointBuffer;
char distanceBuffer[32], *distanceValue = distanceBuffer;
int frequencyValue = 0;
int transmitterPowerValue = 0;
int antennaGainValue = 0;
char stateConnection[32] = "";

// ============ SNMP CONFIGURATION AND CALLBACK DEFINITIONS =============

const char* snmpCommunity       = "public";
const char* oidRSSI             = ".1.3.6.1.4.1.17713.21.1.2.3.0";
const char* oidSNR              = ".1.3.6.1.4.1.17713.21.1.2.18.0";
const char* oidAccessPoint      = ".1.3.6.1.4.1.17713.21.1.2.8.0";
const char* oidDistance         = ".1.3.6.1.4.1.17713.21.1.2.12.0";
const char* oidFrequency        = ".1.3.6.1.4.1.17713.21.1.2.20.1.4.1";
const char* oidTransmitterPower = ".1.3.6.1.4.1.17713.21.3.8.2.6.0";
const char* oidAntennaGain      = ".1.3.6.1.4.1.17713.21.3.8.2.7.0";

WiFiUDP udp;

SNMPManager snmpManager(snmpCommunity);
SNMPGet snmpRequest(snmpCommunity, 1);

ValueCallback* rssiCallback;
ValueCallback* snrCallback;
ValueCallback* accessPointCallback;
ValueCallback* distanceCallback;
ValueCallback* frequencyCallback;
ValueCallback* transmitterPowerCallback;
ValueCallback* antennaGainCallback;

// ==================== ETHERNET CONNECTION STATUS =====================

static bool ethernetConnected = false;

// =============== FINITE STATE MACHINE TIMER VARIABLES ================

unsigned long fsmTimerStart = 0;
const unsigned long fsmInterval = 1000;
unsigned long now = millis();


// ================= UART INTERFACE INITIALIZATION =====================

HardwareSerial uart2(2);



// ========================== CORE FUNCTIONS ===========================
// ================= FINITE STATE MACHINE DEFINITIONS ==================

enum State {
  STATE_IDLE,
  STATE_PING,
  STATE_PING_SUCCESS_UART_SEND,
  STATE_PING_TIMEOUT_UART_SEND,
  STATE_SNMP_REQUEST,
  STATE_SNMP_WAIT,
  STATE_SNMP_READ,
  STATE_EVALUATE_RADIO_LINK_QUALITY,
  STATE_RADIO_LINK_QUALITY_UART_SEND
};

State currentState = STATE_IDLE;

// =========================== SETUP FUNCTION ==========================

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();

  pinMode(LED_PIN, OUTPUT);
  blinkLed = false;
  ledSolidOn = true;

  uart2.begin(9600, SERIAL_8N1, 16, 17);

  setupEthernet();

  xTaskCreatePinnedToCore(taskBlinkLED, "taskBlinkLED", 1024, NULL, 1, NULL, 0);
}

// ============================ LOOP FUNCTION ===========================

void loop() {
  stateMachineLoop();
}

// =========================== UTILITY FUNCTION =========================

void setupEthernet() {
  Network.onEvent(onEvent);
  ETH.begin(ETH_PHY_W5500, 1, ETH_CS, ETH_IRQ, ETH_RST, SPI2_HOST, ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
}

void setupSNMP() {
  snmpManager.setUDP(&udp);
  snmpManager.begin();
  rssiCallback              = snmpManager.addIntegerHandler(radioLinkTower, oidRSSI, &rssiValue);
  snrCallback               = snmpManager.addIntegerHandler(radioLinkTower, oidSNR, &snrValue);
  accessPointCallback       = snmpManager.addStringHandler(radioLinkTower, oidAccessPoint, &accessPointValue);
  distanceCallback          = snmpManager.addStringHandler(radioLinkTower, oidDistance, &distanceValue);
  frequencyCallback         = snmpManager.addIntegerHandler(radioLinkTower, oidFrequency, &frequencyValue);
  transmitterPowerCallback  = snmpManager.addIntegerHandler(radioLinkTower, oidTransmitterPower, &transmitterPowerValue);
  antennaGainCallback       = snmpManager.addIntegerHandler(radioLinkTower, oidAntennaGain, &antennaGainValue);
}

void onEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      Serial.println(F("[ETHERNET] Started"));
      ETH.setHostname("esp32-eth0");

      if (!ETH.config(localIP, gateway, subnet)) {
        Serial.println(F("\n[ETHERNET] Failed to configure Static IP"));
        
        esp_restart();
      }

      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      Serial.println(F("\n[ETHERNET] Connected"));
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      ethernetConnected = true;

      Serial.println(F("[ETHERNET] Got IP"));
      Serial.printf("[ETHERNET] IP Address     : %s\n", ETH.localIP().toString().c_str());
      Serial.printf("[ETHERNET] Gateway IP     : %s\n", ETH.gatewayIP().toString().c_str());
      Serial.printf("[ETHERNET] Subnet Mask    : %s\n", ETH.subnetMask().toString().c_str());
      Serial.printf("[ETHERNET] DNS Server     : %s", ETH.dnsIP(0).toString().c_str());
      Serial.println();

      blinkLed = true;
      ledSolidOn = false;

      setupSNMP();
      
      fsmTimerStart = millis();
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      Serial.println(F("\n[ETHERNET] Lost IP\n"));
      ethernetConnected = false;

      esp_restart();
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
    Serial.println(F("\n[ETHERNET] Disconnected\n"));
      ethernetConnected = false;

      esp_restart();
      break;
    case ARDUINO_EVENT_ETH_STOP:
      Serial.println(F("\n[ETHERNET] Stopped\n"));
      ethernetConnected = false;

      esp_restart();
      break;
    default:
      break;
  }
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

// ===================== FINITE STATE MACHINE FUNCTION ==================

void stateMachineLoop() {
  if (!ethernetConnected) return;

  now = millis();

  switch (currentState) {
    case STATE_IDLE:                          handleIdle();                       break;
    case STATE_PING:                          handlePing();                       break;
    case STATE_PING_SUCCESS_UART_SEND:        handlePingSuccessUartSend();        break;
    case STATE_PING_TIMEOUT_UART_SEND:        handlePingTimeoutUartSend();        break;
    case STATE_SNMP_REQUEST:                  handleSnmpRequest();                break;
    case STATE_SNMP_WAIT:                     handleSnmpWait();                   break;
    case STATE_SNMP_READ:                     handleSnmpRead();                   break;
    case STATE_EVALUATE_RADIO_LINK_QUALITY:   handleEvaluateRadioLinkQuality();   break;
    case STATE_RADIO_LINK_QUALITY_UART_SEND:  handleRadioLinkQualityUartSend();   break;
  }
}

void handleIdle() {
  if (now - fsmTimerStart >= fsmInterval) {
    currentState = STATE_PING;
    fsmTimerStart = now;
  }
}

void handlePing() {
  if (Ping.ping(radioLinkTower, 1)) {
    Serial.print(F("\n[PING] Ping to "));
    Serial.print(radioLinkTower);
    Serial.print(F(": time="));
    Serial.print(Ping.averageTime());
    Serial.println(F(" ms\n"));

    currentState = STATE_PING_SUCCESS_UART_SEND;
  } else {
    Serial.println(F("\n[PING] Destination Host Unreachable"));
    
    currentState = STATE_PING_TIMEOUT_UART_SEND;
  }
  fsmTimerStart = now;
}

void handlePingSuccessUartSend() {
  uart2.print("Radio Link Status : Destination Host Reachable\n");
  Serial.println(F("[UART] Destination Host Reachable"));

  currentState = STATE_SNMP_REQUEST;
  fsmTimerStart = now;
}

void handlePingTimeoutUartSend() {
  uart2.print("Radio Link Status : Destination Host Unreachable\n");
  Serial.println(F("\n[UART] Destination Host Unreachable"));

  currentState = STATE_IDLE;
  fsmTimerStart = now;
}

void handleSnmpRequest() {
  snmpRequest.addOIDPointer(rssiCallback);
  snmpRequest.addOIDPointer(snrCallback);
  snmpRequest.addOIDPointer(accessPointCallback);
  snmpRequest.addOIDPointer(distanceCallback);
  snmpRequest.addOIDPointer(frequencyCallback);
  snmpRequest.addOIDPointer(transmitterPowerCallback);
  snmpRequest.addOIDPointer(antennaGainCallback);

  snmpRequest.setIP(ETH.localIP());
  snmpRequest.setUDP(&udp);
  snmpRequest.setRequestID(random(10000));
  snmpRequest.sendTo(radioLinkTower);
  snmpRequest.clearOIDList();

  currentState = STATE_SNMP_WAIT;
  fsmTimerStart = now;
}

void handleSnmpWait() {
  if (now - fsmTimerStart >= 1000) {
    currentState = STATE_SNMP_READ;
    fsmTimerStart = now;
  }
}

void handleSnmpRead() {
  snmpManager.loop();
  
  rssiValue = (rssiValue > 127) ? rssiValue - 256 : rssiValue;

  Serial.print("\n[SNMP] RSSI               : "); Serial.print(rssiValue);              Serial.println(" dBm");
  Serial.print("[SNMP] SNR                : ");   Serial.print(snrValue);               Serial.println(" dB");
  Serial.print("[SNMP] Access Point       : ");   Serial.println(accessPointValue); 
  Serial.print("[SNMP] Distance           : ");   Serial.print(distanceValue);          Serial.println(" Km");
  Serial.print("[SNMP] Frequency          : ");   Serial.print(frequencyValue);         Serial.println(" MHz");
  Serial.print("[SNMP] Transmitter Power  : ");   Serial.print(transmitterPowerValue);  Serial.println(" dBm");
  Serial.print("[SNMP] Antenna Gain       : ");   Serial.print(antennaGainValue);       Serial.println(" dBi\n");

  currentState = STATE_EVALUATE_RADIO_LINK_QUALITY;
  fsmTimerStart = now;
}

void handleEvaluateRadioLinkQuality() {
  if (strcmp(accessPointValue, "DVOR-AP") != 0) {
    strcpy(stateConnection, "Koneksi Terputus");
    Serial.print(F("[EVAL] "));
    Serial.println(stateConnection);
  } else {
    if (rssiValue > -70) {
      strcpy(stateConnection, "Koneksi Sangat Baik");
    } else if (rssiValue >= -85) {
      strcpy(stateConnection, "Koneksi Baik");
    } else if (rssiValue >= -100) {
      strcpy(stateConnection, "Koneksi Cukup");
    } else {
      strcpy(stateConnection, "Koneksi Buruk");
    }
    Serial.print(F("[EVAL] "));
    Serial.println(stateConnection);
  }

  currentState = STATE_RADIO_LINK_QUALITY_UART_SEND;
  fsmTimerStart = now;
}

void handleRadioLinkQualityUartSend() {
  String message = "";
  message += "RSSI : "              + String(rssiValue)             + "\n";
  message += "SNR : "               + String(snrValue)              + "\n";
  message += "Access Point : "      + String(accessPointValue)      + "\n";
  message += "Distance : "          + String(distanceValue)         + "\n";
  message += "Frequency : "         + String(frequencyValue)        + "\n";
  message += "Transmitter Power : " + String(transmitterPowerValue) + "\n";
  message += "Antenna Gain : "      + String(antennaGainValue)      + "\n";
  message += "Status : "            + String(stateConnection);      + "\n";

  uart2.println(message);

  Serial.println();

  int startIdx = 0;
  while (startIdx < message.length()) {
    int endIdx = message.indexOf('\n', startIdx);
    if (endIdx == -1) endIdx = message.length();
    String line = message.substring(startIdx, endIdx);
    Serial.println("[UART] " + line);
    startIdx = endIdx + 1;
  }

  currentState = STATE_IDLE;
  fsmTimerStart = now;
}
