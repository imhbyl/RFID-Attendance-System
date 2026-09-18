/*
  RFID Attendance System
  -----------------------
  Arduino UNO + MFRC522 + DS1307 RTC + 16x2 LCD + LEDs + buzzer

  Features:
    - Master Card menu for adding/deleting users (no reprogramming needed)
    - Users stored in EEPROM (persist across power loss)
    - Late-arrival detection against a configurable cutoff time
    - Duplicate-scan cooldown
    - CSV output over Serial: Name,AdmissionNo,RollNo,UID,Date,Time,Status

  Master Card usage:
    Tap Master Card -> type A (add) or D (delete) in Serial Monitor ->
    tap the target card -> for "add", type Name,AdmissionNo,RollNo

  Startup:
    Type RESET within 5 seconds of power-on to erase all stored users.
*/

#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <string.h>
#include <ctype.h>

// ---- RFID reader ----
#define SS_PIN 10
#define RST_PIN 9
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ---- RTC ----
RTC_DS1307 rtc;

// ---- LCD (RS, E, D4, D5, D6, D7) ----
LiquidCrystal lcd(8, 7, 6, 5, 4, 3);

// ---- LEDs + buzzer ----
const int GREEN_LED = A0;
const int RED_LED   = A1;
const int BUZZER    = A2;

// ---- Master Card ----
byte MASTER_UID[4] = {0xAE, 0x52, 0x99, 0x04};

// ---- Late cutoff (24-hour format) ----
const int LATE_HOUR   = 9;
const int LATE_MINUTE = 0;

// ---- User storage (EEPROM-backed) ----
struct User {
  byte uid[4];
  char name[13];
  char admissionNo[6];
  char rollNo[3];
};

const int MAX_USERS = 8;
const int EEPROM_COUNT_ADDR = 0;
const int EEPROM_USERS_START = 1;
const int USER_RECORD_SIZE = sizeof(User);

User users[MAX_USERS];
unsigned long lastSeen[MAX_USERS];   // runtime only, not persisted
int userCount = 0;

const unsigned long COOLDOWN_MS = 10000;


// ---------------------------------------------------
// EEPROM helpers
// ---------------------------------------------------

void loadUsersFromEEPROM() {
  userCount = EEPROM.read(EEPROM_COUNT_ADDR);
  if (userCount < 0 || userCount > MAX_USERS) userCount = 0;

  for (int i = 0; i < userCount; i++) {
    EEPROM.get(EEPROM_USERS_START + (i * USER_RECORD_SIZE), users[i]);
    lastSeen[i] = 0;
  }
}

void saveUserToEEPROM(int index, User &u) {
  EEPROM.put(EEPROM_USERS_START + (index * USER_RECORD_SIZE), u);
  EEPROM.write(EEPROM_COUNT_ADDR, userCount);
}


int readSerialLine(char* buf, int maxLen) {
  int len = Serial.readBytesUntil('\n', buf, maxLen - 1);
  buf[len] = '\0';
  while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == ' ')) {
    buf[--len] = '\0';
  }
  return len;
}


// ---------------------------------------------------
// UID matching
// ---------------------------------------------------

bool sameUID(byte* a, byte* b) {
  for (byte i = 0; i < 4; i++) {
    if (a[i] != b[i]) return false;
  }
  return true;
}

int findUser(byte* uid) {
  for (int i = 0; i < userCount; i++) {
    if (sameUID(uid, users[i].uid)) return i;
  }
  return -1;
}

void deleteUser(int index) {
  for (int i = index; i < userCount - 1; i++) {
    users[i] = users[i + 1];
    lastSeen[i] = lastSeen[i + 1];
    saveUserToEEPROM(i, users[i]);
  }
  userCount--;
  EEPROM.write(EEPROM_COUNT_ADDR, userCount);
}


// ---------------------------------------------------
// Enrollment
// ---------------------------------------------------

void enrollNewUser(byte* uid) {
  if (userCount >= MAX_USERS) {
    lcd.clear();
    lcd.print("Storage FULL");
    Serial.println("ERROR: No space for more users.");
    delay(2000);
    return;
  }

  if (findUser(uid) != -1) {
    lcd.clear();
    lcd.print("Already exists");
    delay(1500);
    return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Check Serial");
  lcd.setCursor(0, 1);
  lcd.print("Monitor now...");

  Serial.println("Type: Name,AdmissionNo,RollNo   then press Enter");

  while (!Serial.available()) {
    delay(50);
  }

  char inputBuf[32];
  readSerialLine(inputBuf, sizeof(inputBuf));

  char* namePart = strtok(inputBuf, ",");
  char* admPart  = strtok(NULL, ",");
  char* rollPart = strtok(NULL, ",");

  if (!namePart || !admPart || !rollPart) {
    Serial.println("ERROR: Wrong format. Enrollment cancelled.");
    lcd.clear();
    lcd.print("Enroll Failed");
    delay(1500);
    return;
  }

  User u;
  memcpy(u.uid, uid, 4);
  strncpy(u.name, namePart, sizeof(u.name) - 1);
  u.name[sizeof(u.name) - 1] = '\0';
  strncpy(u.admissionNo, admPart, sizeof(u.admissionNo) - 1);
  u.admissionNo[sizeof(u.admissionNo) - 1] = '\0';
  strncpy(u.rollNo, rollPart, sizeof(u.rollNo) - 1);
  u.rollNo[sizeof(u.rollNo) - 1] = '\0';

  users[userCount] = u;
  lastSeen[userCount] = 0;
  userCount++;
  saveUserToEEPROM(userCount - 1, u);

  Serial.print("Enrolled: ");
  Serial.println(u.name);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enrolled:");
  lcd.setCursor(0, 1);
  lcd.print(u.name);
  delay(2000);
}


// ---------------------------------------------------
// LCD display for a valid scan
// ---------------------------------------------------

void showAttendance(User &user, DateTime now, bool isLate) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Welcome");
  lcd.setCursor(0, 1);
  lcd.print(user.name);
  delay(1200);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Adm:");
  lcd.print(user.admissionNo);
  lcd.setCursor(0, 1);
  lcd.print("Roll:");
  lcd.print(user.rollNo);
  delay(1200);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(isLate ? "Status: LATE" : "Status: ON TIME");
  lcd.setCursor(0, 1);
  if (now.hour() < 10) lcd.print("0");
  lcd.print(now.hour());
  lcd.print(":");
  if (now.minute() < 10) lcd.print("0");
  lcd.print(now.minute());
  delay(1500);
}


// ---------------------------------------------------
// Setup
// ---------------------------------------------------

void setup() {
  Serial.begin(9600);

  SPI.begin();
  mfrc522.PCD_Init();

  Wire.begin();
  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("Starting...");
  delay(1000);

  if (!rtc.begin()) {
    lcd.clear();
    lcd.print("RTC ERROR!");
    Serial.println("ERROR: DS1307 not found.");
    while (1);
  }

  if (!rtc.isrunning()) {
    lcd.clear();
    lcd.print("RTC NOT RUNNING");
    Serial.println("WARNING: RTC is not running.");
    delay(2000);
  }

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);

  loadUsersFromEEPROM();
  Serial.print("Loaded ");
  Serial.print(userCount);
  Serial.println(" users from EEPROM.");

  // Optional full reset window
  Serial.println("Type RESET within 5 seconds to erase all stored users.");
  unsigned long resetWindowStart = millis();
  while (millis() - resetWindowStart < 5000) {
    if (Serial.available()) {
      char cmdBuf[8];
      readSerialLine(cmdBuf, sizeof(cmdBuf));
      if (strcasecmp(cmdBuf, "RESET") == 0) {
        userCount = 0;
        EEPROM.write(EEPROM_COUNT_ADDR, 0);
        Serial.println("All users erased.");
        lcd.clear();
        lcd.print("All users wiped");
        delay(1500);
      }
      break;
    }
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Attendance");
  lcd.setCursor(0, 1);
  lcd.print("System Ready");
  delay(1500);

  lcd.clear();
  lcd.print("Scan your card");
}


// ---------------------------------------------------
// Main loop
// ---------------------------------------------------

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  if (mfrc522.uid.size != 4) {
    lcd.clear();
    lcd.print("Unsupported card");
    digitalWrite(RED_LED, HIGH);
    tone(BUZZER, 300, 400);
    delay(1200);
    digitalWrite(RED_LED, LOW);
    lcd.clear();
    lcd.print("Scan your card");
    mfrc522.PICC_HaltA();
    return;
  }

  // ---- Master Card menu ----
  if (sameUID(mfrc522.uid.uidByte, MASTER_UID)) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MASTER CARD");
    lcd.setCursor(0, 1);
    lcd.print("Check Serial");
    tone(BUZZER, 800, 150);
    mfrc522.PICC_HaltA();
    mfrc522.PCD_Init();
    delay(100);

    Serial.println("Type A to Add or D to Delete, then press Enter");

    unsigned long waitStart = millis();
    while (!Serial.available()) {
      if (millis() - waitStart > 15000) {
        lcd.clear();
        lcd.print("Timeout");
        delay(1200);
        lcd.clear();
        lcd.print("Scan your card");
        return;
      }
      delay(50);
    }

    char choiceBuf[8];
    readSerialLine(choiceBuf, sizeof(choiceBuf));
    char choice = toupper(choiceBuf[0]);

    if (choice == 'A') {
      lcd.clear();
      lcd.print("Tap new card...");
      mfrc522.PCD_Init();
      delay(100);

      waitStart = millis();
      while (true) {
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
          byte newUID[4];
          memcpy(newUID, mfrc522.uid.uidByte, 4);
          enrollNewUser(newUID);
          mfrc522.PICC_HaltA();
          mfrc522.PCD_Init();
          delay(100);
          break;
        }
        if (millis() - waitStart > 15000) {
          lcd.clear();
          lcd.print("Enroll Timeout");
          delay(1200);
          break;
        }
      }

    } else if (choice == 'D') {
      lcd.clear();
      lcd.print("Tap card to del");
      mfrc522.PCD_Init();
      delay(100);

      waitStart = millis();
      while (true) {
        if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
          int idx = findUser(mfrc522.uid.uidByte);
          if (idx == -1) {
            lcd.clear();
            lcd.print("Not Found");
            Serial.println("That card isn't registered.");
          } else {
            char removedName[13];
            strncpy(removedName, users[idx].name, sizeof(removedName));
            deleteUser(idx);
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Deleted:");
            lcd.setCursor(0, 1);
            lcd.print(removedName);
            Serial.print("Deleted: ");
            Serial.println(removedName);
          }
          delay(1500);
          mfrc522.PICC_HaltA();
          mfrc522.PCD_Init();
          delay(100);
          break;
        }
        if (millis() - waitStart > 15000) {
          lcd.clear();
          lcd.print("Delete Timeout");
          delay(1200);
          break;
        }
      }

    } else {
      lcd.clear();
      lcd.print("Unknown choice");
      delay(1200);
    }

    lcd.clear();
    lcd.print("Scan your card");
    return;
  }

  // ---- Normal attendance scan ----
  int matchIndex = findUser(mfrc522.uid.uidByte);

  if (matchIndex == -1) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Not Registered");
    lcd.setCursor(0, 1);
    lcd.print("Access Denied");
    digitalWrite(RED_LED, HIGH);
    tone(BUZZER, 300, 500);
    delay(1200);
    digitalWrite(RED_LED, LOW);

  } else {
    unsigned long currentMillis = millis();

    if (lastSeen[matchIndex] != 0 && currentMillis - lastSeen[matchIndex] < COOLDOWN_MS) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(users[matchIndex].name);
      lcd.setCursor(0, 1);
      lcd.print("Already Marked");
      digitalWrite(RED_LED, HIGH);
      tone(BUZZER, 300, 200);
      delay(1200);
      digitalWrite(RED_LED, LOW);

    } else {
      lastSeen[matchIndex] = currentMillis;

      DateTime now = rtc.now();
      bool isLate = (now.hour() > LATE_HOUR) ||
                    (now.hour() == LATE_HOUR && now.minute() > LATE_MINUTE);

      digitalWrite(GREEN_LED, HIGH);
      tone(BUZZER, 1000, 200);

      showAttendance(users[matchIndex], now, isLate);

      // CSV line over Serial
      Serial.print(users[matchIndex].name);        Serial.print(",");
      Serial.print(users[matchIndex].admissionNo);  Serial.print(",");
      Serial.print(users[matchIndex].rollNo);       Serial.print(",");

      for (byte i = 0; i < 4; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) Serial.print("0");
        Serial.print(mfrc522.uid.uidByte[i], HEX);
      }
      Serial.print(",");

      if (now.day() < 10) Serial.print("0");
      Serial.print(now.day()); Serial.print("/");
      if (now.month() < 10) Serial.print("0");
      Serial.print(now.month()); Serial.print("/");
      Serial.print(now.year());
      Serial.print(",");

      if (now.hour() < 10) Serial.print("0");
      Serial.print(now.hour()); Serial.print(":");
      if (now.minute() < 10) Serial.print("0");
      Serial.print(now.minute()); Serial.print(":");
      if (now.second() < 10) Serial.print("0");
      Serial.print(now.second());
      Serial.print(",");

      Serial.println(isLate ? "LATE" : "ON TIME");

      digitalWrite(GREEN_LED, LOW);
    }
  }

  lcd.clear();
  lcd.print("Scan your card");

  mfrc522.PICC_HaltA();
  mfrc522.PCD_Init();

  delay(300);
}
