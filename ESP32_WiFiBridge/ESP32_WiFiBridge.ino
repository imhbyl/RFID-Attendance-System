#include <WiFi.h>
#include <HTTPClient.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <SD.h>

// ---------- Wi-Fi ----------
const char* WIFI_SSID = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ---------- Google Apps Script ----------
const char* WEB_APP_URL = "YOUR_GOOGLE_APPS_SCRIPT_URL";

// ---------- Uno serial connection ----------
HardwareSerial UnoSerial(2);

#define UNO_RX_PIN 16   // ESP32 receives Uno data here
#define UNO_TX_PIN 17   // ESP32 sends data to Uno here

// ---------- SD card ----------
#define SD_CS_PIN 5

bool sdReady = false;
unsigned long lastWiFiAttempt = 0;
const unsigned long WIFI_RETRY_MS = 10000;

void setup() {
  Serial.begin(115200);

  // Connection from Uno: Uno A3 -> ESP32 GPIO16
  UnoSerial.begin(9600, SERIAL_8N1, UNO_RX_PIN, UNO_TX_PIN);

  setupSDCard();
  startWiFi();
}

void loop() {
  // Receive attendance records from Uno
  if (UnoSerial.available()) {
    String line = UnoSerial.readStringUntil('\n');
    line.trim();

    if (line.length() > 0) {
      Serial.print("Received from Uno: ");
      Serial.println(line);

      // Save every scan to the SD card first
      saveAttendanceBackup(line);

      // Then try to upload it to Google Sheets
      String result = sendToGoogleSheet(line);
      saveSyncStatus(line, result);

      Serial.print("Cloud result: ");
      Serial.println(result);
    }
  }

  // Keep trying Wi-Fi without stopping RFID attendance scanning
  if (WiFi.status() != WL_CONNECTED &&
      millis() - lastWiFiAttempt >= WIFI_RETRY_MS) {
    startWiFi();
  }
}

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

  File file = SD.open("/attendance_backup.csv", FILE_APPEND);

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
  int partCount = splitCSV(csvLine, parts, 9);

  if (partCount != 9) return;

  File file = SD.open("/sync_status.csv", FILE_APPEND);

  if (!file) {
    Serial.println("Could not save cloud status.");
    return;
  }

  file.println(
    parts[6] + "," + parts[7] + "," +
    parts[0] + "," + parts[1] + "," + result
  );

  file.close();
}

void startWiFi() {
  lastWiFiAttempt = millis();

  if (WiFi.status() == WL_CONNECTED) return;

  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

String sendToGoogleSheet(String csvLine) {
  if (WiFi.status() != WL_CONNECTED) {
    return "PENDING_OFFLINE";
  }

  // Expected:
  // Name,AdmissionNo,RollNo,Class,ParentEmail,UID,Date,Time,Status
  String parts[9];
  int partCount = splitCSV(csvLine, parts, 9);

  if (partCount != 9) {
    return "INVALID_RECORD";
  }

  String url = String(WEB_APP_URL) +
    "?name=" + urlEncode(parts[0]) +
    "&adm=" + urlEncode(parts[1]) +
    "&roll=" + urlEncode(parts[2]) +
    "&class=" + urlEncode(parts[3]) +
    "&email=" + urlEncode(parts[4]) +
    "&date=" + urlEncode(parts[6]) +
    "&time=" + urlEncode(parts[7]) +
    "&status=" + urlEncode(parts[8]);

  HTTPClient http;
  http.begin(url);
  http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);

  int httpCode = http.GET();
  http.end();

  if (httpCode == 200) {
    return "SYNCED";
  }

  return "PENDING_HTTP_" + String(httpCode);
}

int splitCSV(String csvLine, String parts[], int maxParts) {
  int partCount = 0;
  int lastPos = 0;

  for (int i = 0; i <= csvLine.length(); i++) {
    if (i == csvLine.length() || csvLine[i] == ',') {
      if (partCount < maxParts) {
        parts[partCount] = csvLine.substring(lastPos, i);
      }

      partCount++;
      lastPos = i + 1;
    }
  }

  return partCount;
}

String urlEncode(String str) {
  String encoded = "";
  char code[4];

  for (int i = 0; i < str.length(); i++) {
    char c = str.charAt(i);

    if (isalnum(c)) {
      encoded += c;
    } else {
      sprintf(code, "%%%02X", c);
      encoded += code;
    }
  }

  return encoded;
}
