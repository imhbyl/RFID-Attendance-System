#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <SD.h>

// ---------- Wi-Fi ----------
const char* WIFI_SSID = "RIBAL Pro 5G";
const char* WIFI_PASSWORD = "u4qtr86f";

// ---------- Google Apps Script ----------
const char* WEB_APP_URL =
  "https://script.google.com/macros/s/AKfycby44P39FIV6NgFq02QCJqgWlmbk78Bj2jVhOIf0iSKtmDr3WvuSWpWfUW5sGKDSDcIQig/exec";

// ---------- UNO serial ----------
HardwareSerial UnoSerial(2);

#define UNO_RX_PIN 16
#define UNO_TX_PIN 17

// ---------- SD card ----------
#define SD_CS_PIN 5

bool sdReady = false;

unsigned long lastWiFiAttempt = 0;
const unsigned long WIFI_RETRY_MS = 10000;


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(115200);

  // UNO -> ESP32
  UnoSerial.begin(9600, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);

  setupSDCard();

  startWiFi();
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

  // Receive attendance record from UNO
  if (UnoSerial.available()) {

    String line = UnoSerial.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {

      Serial.print("Received from Uno: ");
      Serial.println(line);

      // Always save backup first
      saveAttendanceBackup(line);

      // Upload to Google Sheets
      String result = sendToGoogleSheet(line);

      // Save sync result
      saveSyncStatus(line, result);

      Serial.print("Cloud result: ");
      Serial.println(result);
    }
  }


  // Reconnect Wi-Fi if necessary
  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastWiFiAttempt >= WIFI_RETRY_MS) {

    startWiFi();
  }
}


// =====================================================
// SD CARD
// =====================================================

void setupSDCard() {

  Serial.println("Starting SD card...");

  if (!SD.begin(SD_CS_PIN)) {

    Serial.println("SD card not detected.");
    return;
  }

  sdReady = true;

  Serial.println("SD card ready.");

  addHeaderIfEmpty(
    "/attendance_backup.csv",
    "Name,AdmissionNo,RollNo,Class,ParentEmail,UID,Date,Time,Status"
  );

  addHeaderIfEmpty(
    "/sync_status.csv",
    "Date,Time,Name,AdmissionNo,CloudStatus"
  );
}


void addHeaderIfEmpty(const char* path, const char* header) {

  File file = SD.open(path, FILE_APPEND);

  if (!file) {

    Serial.println("Could not open SD file.");
    return;
  }

  if (file.size() == 0) {
    file.println(header);
  }

  file.close();
}


void saveAttendanceBackup(String csvLine) {

  if (!sdReady) return;

  File file = SD.open(
    "/attendance_backup.csv",
    FILE_APPEND
  );

  if (!file) {

    Serial.println("Could not save SD backup.");
    return;
  }

  file.println(csvLine);
  file.close();

  Serial.println("Saved to SD card.");
}


void saveSyncStatus(String csvLine, String result) {

  if (!sdReady) return;

  String parts[9];

  int partCount = splitCSV(
    csvLine,
    parts,
    9
  );

  if (partCount != 9) return;

  File file = SD.open(
    "/sync_status.csv",
    FILE_APPEND
  );

  if (!file) {

    Serial.println("Could not save cloud status.");
    return;
  }

  file.println(
    parts[6] + "," +
    parts[7] + "," +
    parts[0] + "," +
    parts[1] + "," +
    result
  );

  file.close();
}


// =====================================================
// WIFI
// =====================================================

void startWiFi() {

  lastWiFiAttempt = millis();

  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  Serial.println("Connecting to Wi-Fi...");

  WiFi.begin(
    WIFI_SSID,
    WIFI_PASSWORD
  );
}


// =====================================================
// GOOGLE SHEETS
// =====================================================

String sendToGoogleSheet(String csvLine) {

  if (WiFi.status() != WL_CONNECTED) {

    return "PENDING_OFFLINE";
  }


  // Expected format:
  //
  // Name,
  // AdmissionNo,
  // RollNo,
  // Class,
  // ParentEmail,
  // UID,
  // Date,
  // Time,
  // Status

  String parts[9];

  int partCount = splitCSV(
    csvLine,
    parts,
    9
  );

  if (partCount != 9) {

    return "INVALID_RECORD";
  }


  // Build Google Apps Script URL

  String url =
    String(WEB_APP_URL) +

    "?name=" + urlEncode(parts[0]) +

    "&adm=" + urlEncode(parts[1]) +

    "&roll=" + urlEncode(parts[2]) +

    "&class=" + urlEncode(parts[3]) +

    "&email=" + urlEncode(parts[4]) +

    "&date=" + urlEncode(parts[6]) +

    "&time=" + urlEncode(parts[7]) +

    "&status=" + urlEncode(parts[8]);


  Serial.println("Connecting to Google...");


  // Secure HTTPS client

  WiFiClientSecure client;

  // Used for this project to allow the
  // Google Apps Script HTTPS connection.
  client.setInsecure();


  HTTPClient http;


  if (!http.begin(client, url)) {

    Serial.println("HTTP begin failed.");

    return "HTTP_BEGIN_FAILED";
  }


  // Google Apps Script normally redirects.
  http.setFollowRedirects(
    HTTPC_FORCE_FOLLOW_REDIRECTS
  );

  http.setTimeout(15000);


  // Send request

  int httpCode = http.GET();


  Serial.print("HTTP code: ");
  Serial.println(httpCode);


  // Print Google's response

  if (httpCode > 0) {

    String response = http.getString();

    Serial.print("Google response: ");
    Serial.println(response);
  }


  http.end();


  // Successful request

  if (httpCode == 200) {

    return "SYNCED";
  }


  // Failed request

  return "PENDING_HTTP_" + String(httpCode);
}


// =====================================================
// CSV SPLITTER
// =====================================================

int splitCSV(
  String csvLine,
  String parts[],
  int maxParts
) {

  int partCount = 0;

  int lastPos = 0;


  for (
    int i = 0;
    i <= csvLine.length();
    i++
  ) {

    if (
      i == csvLine.length() ||
      csvLine[i] == ','
    ) {

      if (partCount < maxParts) {

        parts[partCount] =
          csvLine.substring(
            lastPos,
            i
          );
      }

      partCount++;

      lastPos = i + 1;
    }
  }


  return partCount;
}


// =====================================================
// URL ENCODING
// =====================================================

String urlEncode(String str) {

  String encoded = "";

  char code[4];


  for (
    int i = 0;
    i < str.length();
    i++
  ) {

    char c = str.charAt(i);


    // Keep normal letters/numbers

    if (
      (c >= 'a' && c <= 'z') ||
      (c >= 'A' && c <= 'Z') ||
      (c >= '0' && c <= '9') ||
      c == '-' ||
      c == '_' ||
      c == '.' ||
      c == '~'
    ) {

      encoded += c;
    }

    else {

      sprintf(
        code,
        "%%%02X",
        (unsigned char)c
      );

      encoded += code;
    }
  }


  return encoded;
}