/*
  ESP32 — WiFi Bridge for Attendance System
  -------------------------------------------
  Receives a CSV line from the UNO over a wired serial connection,
  then sends it to your Google Apps Script Web App over WiFi.

  UNO sends:  Name,AdmissionNo,RollNo,Class,UID,Date,Time,Status

  Wiring between UNO and ESP32 (IMPORTANT — read this):
    UNO is 5V logic, ESP32 GPIO is 3.3V ONLY. Connecting UNO's TX
    pin directly to ESP32's RX pin can damage the ESP32.

    UNO TX (A3) --[1k resistor]--+--> ESP32 RX (GPIO16)
                                  |
                              [2k resistor]
                                  |
                                 GND
    (This is a simple voltage divider — it drops 5V down to a safe ~3.3V)

    ESP32 TX (GPIO17) --------------> UNO RX (D2)   (no divider needed this way)
    ESP32 GND ------------------------> UNO GND      (must share a common ground)
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>

// ---- WiFi credentials ----
const char* WIFI_SSID = "Wasee";       // <-- CHANGE THIS
const char* WIFI_PASSWORD = "0503200380"; // <-- CHANGE THIS

// ---- Your Google Apps Script Web App URL ----
const char* WEB_APP_URL = "https://script.google.com/macros/s/AKfycby44P39FIV6NgFq02QCJqgWlmbk78Bj2jVhOIf0iSKtmDr3WvuSWpWfUW5sGKDSDcIQig/exec"; // <-- CHANGE THIS

// ---- Serial link to the UNO ----
HardwareSerial UnoSerial(2); // uses ESP32's UART2
#define UNO_RX_PIN 16
#define UNO_TX_PIN 17

void setup() {
  Serial.begin(115200); // for debugging, view this in Serial Monitor
  UnoSerial.begin(9600, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);

  connectToWiFi();
}

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (UnoSerial.available()) {
    String line = UnoSerial.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
      Serial.print("Received from UNO: ");
      Serial.println(line);
      sendToGoogleSheet(line);
    }
  }

  // Reconnect WiFi automatically if it drops
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi dropped — reconnecting...");
    connectToWiFi();
  }
}

void sendToGoogleSheet(String csvLine) {
  // Expected format: Name,AdmissionNo,RollNo,Class,ParentEmail,UID,Date,Time,Status
  String parts[9];
  int partCount = 0;
  int lastPos = 0;
  for (int i = 0; i <= csvLine.length(); i++) {
    if (i == csvLine.length() || csvLine[i] == ',') {
      parts[partCount++] = csvLine.substring(lastPos, i);
      lastPos = i + 1;
    }
  }

  if (partCount != 9) {
    Serial.println("Skipped — unexpected format.");
    return;
  }

  String name   = parts[0];
  String adm    = parts[1];
  String roll   = parts[2];
  String cls    = parts[3];
  String email  = parts[4];
  // parts[5] is UID — not sent to the sheet, but could be added if needed
  String date   = parts[6];
  String time   = parts[7];
  String status = parts[8];

  String url = String(WEB_APP_URL) +
               "?name=" + urlEncode(name) +
               "&adm=" + urlEncode(adm) +
               "&roll=" + urlEncode(roll) +
               "&class=" + urlEncode(cls) +
               "&email=" + urlEncode(email) +
               "&date=" + urlEncode(date) +
               "&time=" + urlEncode(time) +
               "&status=" + urlEncode(status);

  HTTPClient http;
  http.begin(url);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  int httpCode = http.GET();

  Serial.print("Upload result: ");
  Serial.println(httpCode);

  http.end();
}

String urlEncode(String str) {
  String encoded = "";
  char c;
  char code[4];
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (isalnum(c)) {
      encoded += c;
    } else {
      sprintf(code, "%%%02X", c);
      encoded += code;
    }
  }
  return encoded;
}
