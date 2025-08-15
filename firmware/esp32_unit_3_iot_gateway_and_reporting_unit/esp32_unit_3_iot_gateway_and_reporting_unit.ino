// =========================== INCLUDE FILES ===========================
// ======================== LIBRARY INCLUSIONS =========================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <esp_system.h>
#include <WiFiClientSecure.h>
#define ENABLE_SMTP
#include <ReadyMail.h>
#include <time.h>
#include "thingPropertiesVorlinkvue.h"



// =================== DEFINITIONS AND GLOBAL VARIABLES ================
// ===================== NETWORK & SMTP CONFIGURATION ==================

#define WIFI_SSID         SSID
#define WIFI_PASSWORD     PASS
#define AUTHOR_EMAIL      "xxxxxx@gmail.com"
#define AUTHOR_PASSWORD   "xxxx xxxx xxxx xxxx"
#define RECIPIENT_EMAIL   "xxxxxx@gmail.com"
#define SMTP_PORT         465
#define SMTP_HOST         "smtp.gmail.com"
#define DOMAIN_OR_IP      "smtp.gmail.com"
#define SSL_MODE          true
#define AUTHENTICATION    true
#define NOTIFY            "SUCCESS"

WiFiClientSecure ssl_client;
SMTPClient smtp(ssl_client);

bool smtpReady = false;

bool radioLinkConnectedEmailSent = false;
bool radioLinkDisconnectedEmailSent = false;

// ================== GOOGLE WEB APP CONFIGURATION =====================

#define GOOGLE_WEB_APP_URL "https://script.google.com/macros/s/xxxxxxxxx/exec"

HTTPClient http;

char lastStateConnection[32] = "";

// ====================== LED CONTROL VARIABLES ========================

#define LED_PIN 2

bool blinkLed = false;
bool ledSolidOn = false;

// ====================== TIME TRACKING VARIABLES ======================

char currentTime[12];
char currentDate[20];

bool timeValid = false;

// ==================== RADIO LINK DATA CONTAINER ======================

char accessPointValue[32];
float distanceValue = 0.0;
int frequencyValue = 0;
int8_t transmitterPowerValue = 0;
int8_t antennaGainValue = 0;
char stateConnection[32];

// ================= UART INTERFACE INITIALIZATION =====================

HardwareSerial uart2(2);



// ========================== CORE FUNCTIONS ===========================
// ================= FINITE STATE MACHINE DEFINITIONS ==================

enum State {
  STATE_ENSURE_SMTP_CONNECTION,
  STATE_RECEIVE_AND_PROCESS_UART_DATA,
  STATE_SEND_EMAIL_RADIO_LINK_STATUS,
  STATE_SEND_RADIO_LINK_DATA_TO_GSHEET
};

State currentState = STATE_ENSURE_SMTP_CONNECTION;

// ============================ SETUP FUNCTION ==========================

void setup() {
  Serial.begin(115200);
  delay(1500);

  Serial.println();

  pinMode(LED_PIN, OUTPUT);
  blinkLed = false;
  ledSolidOn = true;

  uart2.begin(9600, SERIAL_8N1, 16, 17);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);
  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();

  ssl_client.setInsecure();
  
  xTaskCreatePinnedToCore(taskGetTime, "taskGetTime", 2048, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(taskBlinkLED, "taskBlinkLED", 1024, NULL, 1, NULL, 0);
}

// ============================ LOOP FUNCTION ===========================

void loop() {
  ArduinoCloud.update();
  stateMachineLoop();
}

// =========================== UTILITY FUNCTION =========================

void smtpCb(SMTPStatus status) {
  if (status.text.length()) {
      Serial.printf("[SMTP][%d] %s\n", status.state, status.text.c_str());
  }
}

void sendEmailRadioLinkConnected() {
  SMTPMessage msg;
  msg.headers.add(rfc822_subject, "Radio link is connected - Vorlinkvue");
  msg.headers.add(rfc822_from, "Vorlinkvue <" + String(AUTHOR_EMAIL) + ">");
  msg.headers.add(rfc822_to, "Technical Division <" + String(RECIPIENT_EMAIL) + ">");

  String htmlBody =
    "<!DOCTYPE html>"
    "<html lang='en'>"
    "<head>"
        "<meta charset='UTF-8'>"
        "<title>Vorlinkvue</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<link rel='preconnect' href='https://fonts.googleapis.com'>"
        "<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>"
        "<link href='https://fonts.googleapis.com/css2?family=Bebas+Neue&display=swap' rel='stylesheet'>"
        "<style>"
            "body { font-family: Arial, sans-serif; background-color: #f6f8fa; margin: 0; padding: 0; }"
            ".email-container { max-width: 600px; margin: 0 auto; background-color: #ffffff; padding: 40px 30px; border-radius: 8px; box-shadow: 0 0 10px rgba(0,0,0,0.05); }"
            ".logo { text-align: center; margin-bottom: 20px; }"
            ".vorlinkvue-logo { display: block; margin: 0 auto; flex-direction: column; align-items: center; }"
            ".title { font-size: 24px; font-weight: bold; text-align: center; color: #011d4d; }"
            ".hero-image { text-align: center; }"
            ".content { color: #011d4d; font-size: 15px; line-height: 1.6; }"
            ".content ul { padding-left: 20px; }"
            ".highlight { font-weight: bold; }"
            ".button-container { text-align: center; }"
            ".button { display: inline-block; background-color: #006FFF; color: #ffffff; text-decoration: none; padding: 5px 15px; border-radius: 4px; margin: 5px 0; }"
            ".footer { font-size: 13px; text-align: center; color: #5e6c84; margin-top: 40px; }"
            ".footer a { color: #5e6c84; text-decoration: none; }"
            ".social-icons img { margin: 0 5px; width: 24px; }"
        "</style>"
    "</head>"
    "<body>"
        "<div class='email-container'>"
            "<div class='logo'>"
                "<div class='vorlinkvue-logo'>"
                    "<img src='https://i.imgur.com/nnxoKsQ.png' alt='VORLINKVUE' width='150'>"
                "</div>"
            "</div>"
            "<div class='title'>Radio link is connected</div>"
            "<div class='hero-image'>"
                "<img src='https://i.imgur.com/S26u6he.png' alt='Radio link connected' width='300'>"
            "</div>"
            "<div class='content'>"
                "<p><strong>Dear</strong> Technical Division, AirNav Indonesia Palembang Branch,</p>"
                "<p>We are pleased to inform you that our monitoring system has detected that your radio link connection is <strong>stable and fully operational</strong>. All systems are functioning normally, and no immediate action is required.</p>"
                "<ul>"
                    "<li><strong>Location:</strong> Tower</li>"
                    "<li><strong>Checked on:</strong> " + String(currentDate) + ", " + String(currentTime) + "</li>"
                    "<li><strong>Current Status:</strong> Connected</li>"
                "</ul>"
                "<div class='button-container'>"
                    "<a href='https://docs.google.com/spreadsheets/d/13ywHxxFdURsyKDx27kGtQgONmZpTCYj_vC3VxMpNx6M/edit?usp=sharing' class='button'>View Radio Link Log</a>"
                "</div>"
                "<p><strong>Why this matters</strong></p>"
                "<p>A stable radio link ensures continuous data transmission and seamless network performance across your sites.</p>"
                "<hr>"
                "<p>If you have any questions or require assistance, please <a href='mailto:vorlinkvue@gmail.com'>contact our support team</a>. Our team is available 24/7 to assist you.</p>"
                "<p>Best regards,<br>The Vorlinkvue team</p>"
            "</div>"
            "<div class='footer'>"
                "<p><a href='#'>Privacy Policy</a> • <a href='#'>Contact us</a> • <a href='#'>Support</a></p>"
                "<div class='social-icons'>"
                    "<a href='https://www.instagram.com/ghalib.gh/'><img src='https://i.imgur.com/7Fp8E54.png' alt='Instagram'></a>"
                    "<a href='#'><img src='https://i.imgur.com/JlSEina.png' alt='Facebook'></a>"
                    "<a href='#'><img src='https://i.imgur.com/dT7BEv6.png' alt='Tiktok'></a>"
                "</div>"
                "<p>© Copyright 2025 Vorlinkvue. All rights reserved. We are located at 440 W Merrick Rd, Valley Stream, NY 11580, United States</p>"
                "<div><img src='https://i.imgur.com/nnxoKsQ.png' alt='Company Logo' width='90'></div>"
            "</div>"
        "</div>"
    "</body>"
    "</html>"
  ;
  
  msg.html.body(htmlBody.c_str());
  msg.html.transferEncoding("7bit");
  smtp.send(msg, NOTIFY);
}

void sendEmailRadioLinkDisconnected() {
  SMTPMessage msg;
  msg.headers.add(rfc822_subject, "Radio link is disconnected - Vorlinkvue");
  msg.headers.add(rfc822_from, "Vorlinkvue <" + String(AUTHOR_EMAIL) + ">");
  msg.headers.add(rfc822_to, "Technical Division <" + String(RECIPIENT_EMAIL) + ">");

  String htmlBody =
    "<!DOCTYPE html>"
    "<html lang='en'>"
    "<head>"
        "<meta charset='UTF-8'>"
        "<title>Vorlinkvue</title>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<link rel='preconnect' href='https://fonts.googleapis.com'>"
        "<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>"
        "<link href='https://fonts.googleapis.com/css2?family=Bebas+Neue&display=swap' rel='stylesheet'>"
        "<style>"
            "body{font-family:Arial,sans-serif;background-color:#f6f8fa;margin:0;padding:0;}"
            ".email-container{max-width:600px;margin:0 auto;background-color:#ffffff;padding:40px 30px;border-radius:8px;box-shadow:0 0 10px rgba(0,0,0,0.05);}"
            ".logo{text-align:center;margin-bottom:20px;}"
            ".vorlinkvue-logo{display:block;margin:0 auto;flex-direction:column;align-items:center;}"
            ".title{font-size:24px;font-weight:bold;text-align:center;color:#011d4d;}"
            ".hero-image{text-align:center;}"
            ".content{color:#011d4d;font-size:15px;line-height:1.6;}"
            ".content ul{padding-left:20px;}"
            ".highlight{font-weight:bold;}"
            ".button-container{text-align:center;}"
            ".button{display:inline-block;background-color:#006FFF;color:#ffffff;text-decoration:none;padding:5px 15px;border-radius:4px;margin:5px 0;}"
            ".footer{font-size:13px;text-align:center;color:#5e6c84;margin-top:40px;}"
            ".footer a{color:#5e6c84;text-decoration:none;}"
            ".social-icons img{margin:0 5px;width:24px;}"
        "</style>"
    "</head>"
    "<body>"
        "<div class='email-container'>"
            "<div class='logo'>"
                "<div class='vorlinkvue-logo'>"
                    "<img src='https://i.imgur.com/nnxoKsQ.png' alt='VORLINKVUE' width='150'>"
                "</div>"
            "</div>"
            "<div class='title'>Radio link is disconnected</div>"
            "<div class='hero-image'>"
                "<img src='https://i.imgur.com/gyIutgk.png' alt='Radio link disconnected' width='300'>"
            "</div>"
            "<div class='content'>"
                "<p><strong>Dear</strong> Technical Division, AirNav Indonesia Palembang Branch,</p>"
                "<p>We would like to inform you that our monitoring system has detected a <strong>disconnection</strong> in one of your radio link connections. This interruption may impact your network performance and requires prompt attention to minimize potential downtime and service disruptions.</p>"
                "<ul>"
                    "<li><strong>Location:</strong> Tower</li>"
                    "<li><strong>Detected on:</strong> " + String(currentDate) + ", " + String(currentTime) + "</li>"
                    "<li><strong>Current Status:</strong> Disconnected</li>"
                "</ul>"
                "<div class='button-container'>"
                    "<a href='https://docs.google.com/spreadsheets/d/13ywHxxFdURsyKDx27kGtQgONmZpTCYj_vC3VxMpNx6M/edit?usp=sharing' class='button'>View Radio Link Log</a>"
                "</div>"
                "<p><strong>Why this matters</strong></p>"
                "<p>A stable radio link is critical to ensure uninterrupted data transmission between your sites. Swift action helps maintain operational continuity and prevent extended service disruptions.</p>"
                "<hr>"
                "<p>If you have any questions or require assistance, please <a href='mailto:vorlinkvue@gmail.com'>contact our support team</a>. Our team is available 24/7 to assist you.</p>"
                "<p>Best regards,<br>The Vorlinkvue team</p>"
            "</div>"
            "<div class='footer'>"
                "<p><a href='#'>Privacy Policy</a> • <a href='#'>Contact us</a> • <a href='#'>Support</a></p>"
                "<div class='social-icons'>"
                    "<a href='https://www.instagram.com/ghalib.gh/'><img src='https://i.imgur.com/7Fp8E54.png' alt='Instagram'></a>"
                    "<a href='#'><img src='https://i.imgur.com/JlSEina.png' alt='Facebook'></a>"
                    "<a href='#'><img src='https://i.imgur.com/dT7BEv6.png' alt='Tiktok'></a>"
                "</div>"
                "<p>© Copyright 2025 Vorlinkvue. All rights reserved. We are located at 440 W Merrick Rd, Valley Stream, NY 11580, United States</p>"
                "<div><img src='https://i.imgur.com/nnxoKsQ.png' alt='Company Logo' width='90'></div>"
            "</div>"
        "</div>"
    "</body>"
    "</html>"
  ;

  msg.html.body(htmlBody.c_str());
  msg.html.transferEncoding("7bit");
  smtp.send(msg, NOTIFY);
}

// ========================== FREE RTOS FUNCTION ========================

void taskGetTime(void *parameter) {
  while (!ArduinoCloud.connected()) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }

  while (true) {
    time_t now = ArduinoCloud.getLocalTime();

    if (now == 0) {
      timeValid = false;
    } else {
      timeValid = true;

      struct tm *localTime = localtime(&now);
      if (localTime != NULL) {
        strftime(currentTime, sizeof(currentTime), "%H:%M:%S", localTime);
        strftime(currentDate, sizeof(currentDate), "%d %B %Y", localTime);

        Serial.print(F("\n[TIME] "));
        Serial.print(currentDate);
        Serial.print(F(" "));
        Serial.println(currentTime);
      } else {
        Serial.println(F("[TIME] Gagal konversi waktu"));
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

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
  if (!smtpReady && !timeValid) return;

  blinkLed = true;
  ledSolidOn = false;

  switch (currentState) {
    case STATE_ENSURE_SMTP_CONNECTION:            handleEnsureSmtpConnection();         break;
    case STATE_RECEIVE_AND_PROCESS_UART_DATA:     handleReceiveAndProcessUartData();    break;
    case STATE_SEND_EMAIL_RADIO_LINK_STATUS:      handleSendEmailRadioLinkStatus();     break;
    case STATE_SEND_RADIO_LINK_DATA_TO_GSHEET:    handleSendRadioLinkDataToGsheet();    break;
  }
}

void handleEnsureSmtpConnection() {
  if (!smtp.isConnected()) {
    Serial.println(F("\n[SMTP] Connecting to SMTP..."));
    if (!smtp.connect(SMTP_HOST, SMTP_PORT, DOMAIN_OR_IP, smtpCb, SSL_MODE)) {
      Serial.println(F("[SMTP] Gagal koneksi SMTP."));
      esp_restart();
      return;
    }
    if (AUTHENTICATION && !smtp.authenticate(AUTHOR_EMAIL, AUTHOR_PASSWORD, readymail_auth_password)) {
      Serial.println(F("[SMTP] Gagal autentikasi SMTP."));
      esp_restart();
      return;
    }
    Serial.println(F("[SMTP] Koneksi SMTP dan autentikasi berhasil."));
    smtpReady = true;
  } 

  currentState = STATE_RECEIVE_AND_PROCESS_UART_DATA;
}

void handleReceiveAndProcessUartData() {
  if (uart2.available()) {
    String msg = uart2.readStringUntil('\n');
    msg.trim();
    
    if (msg == "Radio Link Status : Destination Host Reachable") {
      radioLinkReachable = true;
      Serial.println(F("\n[UART] Radio Link Status  : Destination Host Reachable"));
    } else if (msg == "Radio Link Status : Destination Host Unreachable") {
      radioLinkReachable = false;
      Serial.println(F("\n[UART] Radio Link Status  : Destination Host Unreachable"));
    }

    if (radioLinkReachable) {
      if (msg.startsWith("RSSI :")) {
        rssiValue = msg.substring(6).toInt();
        Serial.print("\n[UART] RSSI               : "); Serial.println(rssiValue);
      } else if (msg.startsWith("SNR :")) {
        snrValue = msg.substring(6).toInt();
        Serial.print("[UART] SNR                : "); Serial.println(snrValue);
      } else if (msg.startsWith("Access Point :")) {
        msg.remove(0, 15);
        msg.toCharArray(accessPointValue, 32);
        Serial.print("[UART] Access Point       : "); Serial.println(accessPointValue);
      } else if (msg.startsWith("Distance :")) {
        distanceValue = msg.substring(10).toFloat();
        Serial.print("[UART] Distance           : "); Serial.println(distanceValue);
      } else if (msg.startsWith("Frequency :")) {
        frequencyValue = msg.substring(11).toInt();
        Serial.print("[UART] Frequency          : "); Serial.println(frequencyValue);
      } else if (msg.startsWith("Transmitter Power :")) {
        transmitterPowerValue = msg.substring(19).toInt();
        Serial.print("[UART] Transmitter Power  : "); Serial.println(transmitterPowerValue);
      } else if (msg.startsWith("Antenna Gain :")) {
        antennaGainValue = msg.substring(14).toInt();
        Serial.print("[UART] Antenna Gain       : "); Serial.println(antennaGainValue);
      } else if (msg.startsWith("Status :")) {
        msg.remove(0, 9);
        msg.toCharArray(stateConnection, 32);
        Serial.print("[UART] Status             : ");Serial.println(stateConnection);

        currentState = STATE_SEND_EMAIL_RADIO_LINK_STATUS;
      }
    } else {
      currentState = STATE_ENSURE_SMTP_CONNECTION;
    }
  }
}

void handleSendEmailRadioLinkStatus() {
  if (strcmp(stateConnection, "Koneksi Terputus") == 0) {
    radioLinkConnected = false;

    if (!radioLinkDisconnectedEmailSent) {
      sendEmailRadioLinkDisconnected();
      Serial.println(F("\n[EMAIL] Email Radio Link is Disconnected telah dikirimkan"));
      radioLinkDisconnectedEmailSent = true;
      radioLinkConnectedEmailSent = false;
    }
  } else {
    radioLinkConnected = true;

    if (!radioLinkConnectedEmailSent) {
      sendEmailRadioLinkConnected();
      Serial.println(F("\n[EMAIL] Email Radio Link is Connected telah dikirimkan"));
      radioLinkConnectedEmailSent = true;
      radioLinkDisconnectedEmailSent = false;
    }
  }

  currentState = STATE_SEND_RADIO_LINK_DATA_TO_GSHEET;
}

void handleSendRadioLinkDataToGsheet() {
  if (!(strcmp(stateConnection, "Koneksi Sangat Baik") == 0 ||
        strcmp(stateConnection, "Koneksi Baik") == 0 ||
        strcmp(stateConnection, "Koneksi Cukup") == 0 ||
        strcmp(stateConnection, "Koneksi Buruk") == 0 ||
        strcmp(stateConnection, "Koneksi Terputus") == 0)) {
    currentState = STATE_ENSURE_SMTP_CONNECTION;
    return;
  }

  if (strcmp(stateConnection, lastStateConnection) == 0) {
    currentState = STATE_ENSURE_SMTP_CONNECTION;
    return;
  }

  strcpy(lastStateConnection, stateConnection);

  bool success = false;
  uint8_t attempts = 0;
  const int maxAttempts = 1;

  while (attempts < maxAttempts && !success) {
    http.begin(GOOGLE_WEB_APP_URL);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<256> doc;
    doc["rssi"] = rssiValue;
    doc["snr"] = snrValue;
    doc["ap"] = accessPointValue;
    doc["distance"] = distanceValue;
    doc["frequency"] = frequencyValue;
    doc["tx_power"] = transmitterPowerValue;
    doc["antenna_gain"] = antennaGainValue;
    doc["status"] = stateConnection;

    String jsonData;
    serializeJson(doc, jsonData);

    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode == 200 || httpResponseCode == 302) {
      Serial.println(F("\n[Google Sheet] Berhasil mengirimkan data"));
      success = true;
    } else {
      Serial.print(F("\n[Google Sheet] Gagal mengirimkan data (percobaan ke-"));
      Serial.print(attempts + 1);
      Serial.println(")");
    }

    http.end();
    attempts++;
  }

  currentState = STATE_ENSURE_SMTP_CONNECTION;
}

// ======================= ARDUINO IOT CLOUD FUNCTION ===================

void onEspRestartChange()  {
  if (espRestart) {
    Serial.println("\n[Arduino IOT Cloud] Melakukan restart\n");
    espRestart = false;
    ArduinoCloud.update();
    delay(1000);
    esp_restart();
  }
}
