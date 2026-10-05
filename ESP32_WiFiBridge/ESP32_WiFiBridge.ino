#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <SD.h>

// ======================================================
// WIFI
// ======================================================

const char* WIFI_SSID = "Wasee";
const char* WIFI_PASSWORD = "0503200380";

// ======================================================
// GOOGLE APPS SCRIPT
// ======================================================

const char* GOOGLE_SCRIPT_URL =
  "https://script.google.com/macros/s/AKfycbyH4c5goMI27HBMoGIHw99edwarxdihwTC12l0cF0kuKaT_rl2qRM98Ezo-VvaJUpR8tQ/exec";

// ======================================================
// SD CARD
// ======================================================

#define SD_CS 5

// ESP32 VSPI
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23

// ======================================================
// UNO SERIAL
// ======================================================

#define UNO_RX 16
#define UNO_TX 17

String serialLine = "";

bool sdReady = false;
bool wifiReady = false;

unsigned long lastWiFiCheck = 0;
unsigned long lastQueueCheck = 0;

// ======================================================
// URL ENCODE
// ======================================================

String urlEncode(String str) {

  String encoded = "";

  char c;
  char code0;
  char code1;

  for (unsigned int i = 0; i < str.length(); i++) {

    c = str.charAt(i);

    if (
      isalnum(c) ||
      c == '-' ||
      c == '_' ||
      c == '.' ||
      c == '~'
    ) {

      encoded += c;

    } else {

      code1 = (c & 0x0F) + '0';

      if ((c & 0x0F) > 9)
        code1 = (c & 0x0F) - 10 + 'A';

      c = (c >> 4) & 0x0F;

      code0 = c + '0';

      if (c > 9)
        code0 = c - 10 + 'A';

      encoded += '%';
      encoded += code0;
      encoded += code1;
    }
  }

  return encoded;
}

// ======================================================
// WIFI
// ======================================================

void connectWiFi() {

  Serial.println();
  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long startTime = millis();

  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - startTime < 20000UL
  ) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {

    wifiReady = true;

    Serial.println("WiFi connected!");

    Serial.print("ESP32 IP: ");
    Serial.println(WiFi.localIP());

  } else {

    wifiReady = false;

    Serial.println("WiFi connection failed.");
  }
}

// ======================================================
// GOOGLE UPLOAD
// ======================================================

bool sendToGoogle(const String &record) {

  Serial.println();
  Serial.println("================================");
  Serial.println("UPLOADING TO GOOGLE SHEETS");
  Serial.println("================================");

  Serial.println("RAW UNO RECORD:");
  Serial.println(record);

  // ---------------------------------------------------------
  // Expected UNO record:
  //
  // Name,AdmissionNo,RollNo,Class,Email,UID,Date,Time,Status
  //
  // ---------------------------------------------------------

  String parts[9];

  int start = 0;
  int index = 0;

  for (int i = 0; i <= record.length(); i++) {

    if (i == record.length() || record.charAt(i) == ',') {

      if (index < 9) {

        parts[index] = record.substring(start, i);
        parts[index].trim();

        index++;
      }

      start = i + 1;
    }
  }

  if (index != 9) {

    Serial.println("ERROR: Expected exactly 9 fields.");

    Serial.print("Fields received: ");
    Serial.println(index);

    return false;
  }

  // ---------------------------------------------------------
  // Parse fields
  // ---------------------------------------------------------

  String name        = parts[0];
  String admissionNo = parts[1];
  String rollNo      = parts[2];
  String className   = parts[3];
  String email       = parts[4];

  // UID = parts[5]
  // DO NOT SEND UID TO GOOGLE

  String date   = parts[6];
  String time   = parts[7];
  String status = parts[8];

  // ---------------------------------------------------------
  // SHOW EXACT DATA BEING SENT
  // ---------------------------------------------------------

  Serial.println();
  Serial.println("PARSED DATA:");
  Serial.println("------------------------------");

  Serial.print("Name: ");
  Serial.println(name);

  Serial.print("AdmissionNo: ");
  Serial.println(admissionNo);

  Serial.print("RollNo: ");
  Serial.println(rollNo);

  Serial.print("Class: ");
  Serial.println(className);

  Serial.print("Email: ");
  Serial.println(email);

  Serial.print("UID IGNORED: ");
  Serial.println(parts[5]);

  Serial.print("Date: ");
  Serial.println(date);

  Serial.print("Time: ");
  Serial.println(time);

  Serial.print("Status: ");
  Serial.println(status);

  Serial.println("------------------------------");

  // ---------------------------------------------------------
  // Build GET request
  // ---------------------------------------------------------

  String url = GOOGLE_SCRIPT_URL;

  url += "?name=" + urlEncode(name);
  url += "&adm=" + urlEncode(admissionNo);
  url += "&roll=" + urlEncode(rollNo);
  url += "&class=" + urlEncode(className);
  url += "&email=" + urlEncode(email);
  url += "&date=" + urlEncode(date);
  url += "&time=" + urlEncode(time);
  url += "&status=" + urlEncode(status);

  Serial.println();
  Serial.println("FINAL GOOGLE URL:");
  Serial.println(url);

  // ---------------------------------------------------------
  // HTTPS
  // ---------------------------------------------------------

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  if (!http.begin(client, url)) {

    Serial.println("ERROR: HTTP begin failed.");

    return false;
  }

  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setTimeout(15000);

  Serial.println();
  Serial.println("Sending GET request...");

  int httpCode = http.GET();

  Serial.print("HTTP CODE: ");
  Serial.println(httpCode);

  if (httpCode > 0) {

    String response = http.getString();
    response.trim();

    Serial.println("GOOGLE RESPONSE:");
    Serial.println(response);

    if (response == "OK") {

      Serial.println();
      Serial.println("================================");
      Serial.println("GOOGLE UPLOAD SUCCESS");
      Serial.println("================================");

      http.end();

      return true;
    }

    Serial.println("ERROR: Google returned something other than OK.");

  } else {

    Serial.print("HTTP REQUEST FAILED: ");
    Serial.println(http.errorToString(httpCode));
  }

  http.end();

  Serial.println();
  Serial.println("GOOGLE UPLOAD FAILED");

  return false;
}

// ======================================================
// SAVE OFFLINE
// ======================================================

bool saveOffline(String data) {

  if (!sdReady) {

    Serial.println("SD card unavailable.");

    return false;
  }

  File file = SD.open(
    "/offline_queue.csv",
    FILE_APPEND
  );

  if (!file) {

    Serial.println("Could not open offline queue.");

    return false;
  }

  file.println(data);

  file.close();

  Serial.println("Saved to SD queue.");

  return true;
}

// ======================================================
// UPLOAD OFFLINE QUEUE
// ======================================================

void uploadOfflineQueue() {

  if (!sdReady)
    return;

  if (WiFi.status() != WL_CONNECTED)
    return;

  if (!SD.exists("/offline_queue.csv"))
    return;

  Serial.println();
  Serial.println("==============================");
  Serial.println("Checking offline queue...");
  Serial.println("==============================");

  File input = SD.open(
    "/offline_queue.csv",
    FILE_READ
  );

  if (!input) {

    Serial.println(
      "Could not open offline queue."
    );

    return;
  }

  String failedData = "";

  while (input.available()) {

    String line =
      input.readStringUntil('\n');

    line.trim();

    if (line.length() == 0)
      continue;

    Serial.println();
    Serial.print("Retrying: ");
    Serial.println(line);

    if (sendToGoogle(line)) {

      Serial.println(
        "Offline record uploaded."
      );

    } else {

      Serial.println(
        "Still failed."
      );

      failedData += line;
      failedData += "\n";
    }

    delay(300);
  }

  input.close();

  // Delete old queue
  SD.remove("/offline_queue.csv");

  // Put failed records back
  if (failedData.length() > 0) {

    File output = SD.open(
      "/offline_queue.csv",
      FILE_WRITE
    );

    if (output) {

      output.print(failedData);

      output.close();

      Serial.println(
        "Failed records kept on SD."
      );

    } else {

      Serial.println(
        "Could not recreate offline queue."
      );
    }

  } else {

    Serial.println(
      "Offline queue completely uploaded."
    );
  }
}

// ======================================================
// ATTENDANCE RECEIVED FROM UNO
// ======================================================

void handleAttendance(String data) {

  data.trim();

  if (data.length() == 0)
    return;

  Serial.println();
  Serial.println("==============================");
  Serial.println("ATTENDANCE RECEIVED");
  Serial.println("==============================");

  Serial.println(data);

  // Try Google first
  bool uploaded = sendToGoogle(data);

  // If Google fails, save locally
  if (!uploaded) {

    saveOffline(data);

  } else {

    // Upload any old records waiting on SD
    uploadOfflineQueue();
  }
}

// ======================================================
// READ UNO SERIAL
// ======================================================

void readUNOCommands() {

  while (Serial2.available()) {

    char c = Serial2.read();

    if (c == '\n' || c == '\r') {

      if (serialLine.length() > 0) {

        handleAttendance(serialLine);

        serialLine = "";
      }

    } else {

      serialLine += c;

      if (serialLine.length() > 300) {

        serialLine = "";

        Serial.println(
          "UNO serial buffer overflow - cleared."
        );
      }
    }
  }
}

// ======================================================
// USB SERIAL COMMANDS
// ======================================================

String usbLine = "";

void readUSBCommands() {

  while (Serial.available()) {

    char c = Serial.read();

    if (c == '\n' || c == '\r') {

      if (usbLine.length() > 0) {

        usbLine.trim();

        if (usbLine == "WIFI") {

          connectWiFi();

        } else if (usbLine == "CLEAR") {

          clearOfflineQueue();

        } else if (usbLine == "QUEUE") {

          uploadOfflineQueue();

        } else if (usbLine == "SD") {

          Serial.println();

          if (sdReady)
            Serial.println("SD card is ready.");
          else
            Serial.println("SD card unavailable.");

        } else {

          Serial.println("Commands: WIFI, QUEUE, CLEAR, SD");
        }

        usbLine = "";
      }

    } else {

      usbLine += c;

      if (usbLine.length() > 100)
        usbLine = "";
    }
  }
}

// ======================================================
// SETUP
// ======================================================
void clearOfflineQueue() {

  if (!sdReady) {
    Serial.println("SD card unavailable.");
    return;
  }

  if (SD.exists("/offline_queue.csv")) {

    if (SD.remove("/offline_queue.csv")) {

      Serial.println();
      Serial.println("==============================");
      Serial.println("OFFLINE QUEUE CLEARED");
      Serial.println("==============================");

    } else {

      Serial.println("ERROR: Could not delete offline queue.");

    }

  } else {

    Serial.println("Offline queue is already empty.");

  }
}
void setup() {

  Serial.begin(115200);

  delay(1000);

  Serial.println();
  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 RFID ATTENDANCE BRIDGE");
  Serial.println("NO PIN SYSTEM");
  Serial.println("NO KEYPAD");
  Serial.println("================================");

  // ----------------------------------------------------
  // UNO SERIAL
  // ----------------------------------------------------

  Serial.println(
    "Starting UNO Serial2..."
  );

  Serial2.begin(
    9600,
    SERIAL_8N1,
    UNO_RX,
    UNO_TX
  );

  Serial.println("Serial2 started.");

  Serial.println("UNO connection:");
  Serial.println("UNO TX -> ESP32 GPIO16");
  Serial.println("UNO RX -> ESP32 GPIO17");

  // ----------------------------------------------------
  // SD CARD
  // ----------------------------------------------------

  Serial.println();
  Serial.println("Initializing SD card...");

  SPI.begin(
    SD_SCK,
    SD_MISO,
    SD_MOSI,
    SD_CS
  );

  if (SD.begin(SD_CS, SPI)) {

    sdReady = true;

    Serial.println("SD card OK.");

  } else {

    sdReady = false;

    Serial.println("SD card FAILED.");
    Serial.println(
      "Offline queue unavailable."
    );
  }

  // ----------------------------------------------------
  // WIFI
  // ----------------------------------------------------

  connectWiFi();

  // ----------------------------------------------------
  // STARTUP
  // ----------------------------------------------------

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 READY");
  Serial.println("================================");

  Serial.println();
  Serial.println("Waiting for attendance data...");
  Serial.println();

  // Try uploading old queue immediately
  if (sdReady && wifiReady) {

    uploadOfflineQueue();
  }
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  // Read attendance from UNO
  readUNOCommands();

  // USB commands
  readUSBCommands();

  // ----------------------------------------------------
  // WIFI CHECK
  // ----------------------------------------------------

  if (
    millis() - lastWiFiCheck >=
    30000UL
  ) {

    lastWiFiCheck = millis();

    if (WiFi.status() != WL_CONNECTED) {

      wifiReady = false;

      Serial.println();
      Serial.println(
        "WiFi disconnected. Reconnecting..."
      );

      connectWiFi();

    } else {

      wifiReady = true;
    }
  }

  // ----------------------------------------------------
  // OFFLINE QUEUE CHECK
  // ----------------------------------------------------

  if (
    wifiReady &&
    millis() - lastQueueCheck >=
    30000UL
  ) {

    lastQueueCheck = millis();

    uploadOfflineQueue();
  }

  delay(5);
}