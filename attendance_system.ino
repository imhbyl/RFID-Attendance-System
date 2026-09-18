/*
  RFID ATTENDANCE SYSTEM
  =======================

  Components:
  - Arduino Uno
  - MFRC522 RFID
  - DS1307 RTC
  - 16x2 LCD (parallel)
  - Green LED
  - Red LED
  - Buzzer

  Serial CSV format:
  Name,AdmissionNo,RollNo,UID,Date,Time

  Example:
  Hubayl,20240057,12,567FFA03,18/09/2026,15:30:25
*/

#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <RTClib.h>


// =====================================================
// RFID
// =====================================================

#define SS_PIN 10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);


// =====================================================
// DS1307 RTC
// =====================================================

RTC_DS1307 rtc;


// =====================================================
// LCD
// RS, E, D4, D5, D6, D7
// =====================================================

LiquidCrystal lcd(8, 7, 6, 5, 4, 3);


// =====================================================
// LEDs + BUZZER
// =====================================================

const int GREEN_LED = A0;
const int RED_LED   = A1;
const int BUZZER    = A2;


// =====================================================
// USER INFORMATION
// =====================================================
//
// YOUR CARD UID:
// 56 7F FA 03
//
// Change ONLY the name/admission/roll numbers below.
// =====================================================

struct User {
  byte uid[4];
  const char* name;
  const char* admissionNo;
  const char* rollNo;
  unsigned long lastSeen;
};


User users[] = {

  {
    {0x56, 0x7F, 0xFA, 0x03},
    "Hubayl Wasee",
    "69254",     // CHANGE THIS
    "02",           // CHANGE THIS
    0
  }

};


const int NUM_USERS = sizeof(users) / sizeof(users[0]);


// =====================================================
// DUPLICATE SCAN COOLDOWN
// =====================================================

const unsigned long COOLDOWN_MS = 10000;


// =====================================================
// COMPARE RFID UID
// =====================================================

bool sameUID(byte* cardUID, byte* registeredUID) {

  for (byte i = 0; i < 4; i++) {

    if (cardUID[i] != registeredUID[i]) {
      return false;
    }

  }

  return true;
}


// =====================================================
// SHOW ATTENDANCE INFORMATION ON LCD
// =====================================================

void showAttendance(User &user, DateTime now) {

  // -----------------------------------------------
  // SCREEN 1
  // Name
  // -----------------------------------------------

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Welcome");

  lcd.setCursor(0, 1);
  lcd.print(user.name);

  delay(1500);


  // -----------------------------------------------
  // SCREEN 2
  // Admission + Roll
  // -----------------------------------------------

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Adm:");
  lcd.print(user.admissionNo);

  lcd.setCursor(0, 1);
  lcd.print("Roll:");
  lcd.print(user.rollNo);

  delay(1500);


  // -----------------------------------------------
  // SCREEN 3
  // Date
  // -----------------------------------------------

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Date:");

  lcd.setCursor(0, 1);

  if (now.day() < 10) {
    lcd.print("0");
  }

  lcd.print(now.day());
  lcd.print("/");

  if (now.month() < 10) {
    lcd.print("0");
  }

  lcd.print(now.month());
  lcd.print("/");
  lcd.print(now.year());

  delay(1500);


  // -----------------------------------------------
  // SCREEN 4
  // Time
  // -----------------------------------------------

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Time:");

  lcd.setCursor(0, 1);

  if (now.hour() < 10) {
    lcd.print("0");
  }

  lcd.print(now.hour());
  lcd.print(":");

  if (now.minute() < 10) {
    lcd.print("0");
  }

  lcd.print(now.minute());
  lcd.print(":");

  if (now.second() < 10) {
    lcd.print("0");
  }

  lcd.print(now.second());

  delay(1500);
}


// =====================================================
// SETUP
// =====================================================

void setup() {

  Serial.begin(9600);

  // -----------------------------
  // RFID
  // -----------------------------

  SPI.begin();
  mfrc522.PCD_Init();


  // -----------------------------
  // RTC
  // -----------------------------

  Wire.begin();

  lcd.begin(16, 2);

  lcd.clear();
  lcd.print("Starting...");
  delay(1000);


  if (!rtc.begin()) {

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("RTC ERROR!");

    lcd.setCursor(0, 1);
    lcd.print("Check wiring");

    Serial.println("ERROR: DS1307 not found.");

    while (1);
  }


  // Check whether RTC is running

  if (!rtc.isrunning()) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("RTC NOT RUNNING");

    Serial.println("WARNING: RTC is not running.");

    delay(2000);

    /*
      IMPORTANT:

      Do NOT automatically set the RTC every time
      the Arduino starts.

      If you need to set the RTC time, use a separate
      RTC-setting sketch.
    */
  }


  // -----------------------------
  // LEDs + buzzer
  // -----------------------------

  pinMode(GREEN_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);


  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(BUZZER, LOW);


  // -----------------------------
  // Startup message
  // -----------------------------

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Attendance");

  lcd.setCursor(0, 1);
  lcd.print("System Ready");

  Serial.println("Attendance system ready.");

  delay(2000);

  lcd.clear();

  lcd.print("Scan your card");
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {


  // ===================================================
  // CHECK FOR RFID CARD
  // ===================================================

  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }


  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }


  // ===================================================
  // MAKE SURE UID IS 4 BYTES
  // ===================================================

  if (mfrc522.uid.size != 4) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Unsupported");

    lcd.setCursor(0, 1);
    lcd.print("Card UID");

    digitalWrite(RED_LED, HIGH);

    tone(BUZZER, 300, 400);

    delay(1500);

    digitalWrite(RED_LED, LOW);

    lcd.clear();
    lcd.print("Scan your card");

    mfrc522.PICC_HaltA();

    return;
  }


  // ===================================================
  // FIND USER
  // ===================================================

  int matchIndex = -1;


  for (int i = 0; i < NUM_USERS; i++) {

    if (sameUID(mfrc522.uid.uidByte, users[i].uid)) {

      matchIndex = i;

      break;
    }
  }


  // ===================================================
  // USER NOT REGISTERED
  // ===================================================

  if (matchIndex == -1) {

    lcd.clear();

    lcd.setCursor(0, 0);
    lcd.print("Not Registered");

    lcd.setCursor(0, 1);
    lcd.print("Access Denied");

    digitalWrite(RED_LED, HIGH);

    tone(BUZZER, 300, 500);

    delay(1500);

    digitalWrite(RED_LED, LOW);

  }


  // ===================================================
  // REGISTERED USER
  // ===================================================

  else {

    unsigned long currentMillis = millis();


    // -------------------------------------------------
    // DUPLICATE SCAN CHECK
    // -------------------------------------------------

    if (
      users[matchIndex].lastSeen != 0 &&
      currentMillis - users[matchIndex].lastSeen < COOLDOWN_MS
    ) {

      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print(users[matchIndex].name);

      lcd.setCursor(0, 1);
      lcd.print("Already Marked");

      digitalWrite(RED_LED, HIGH);

      tone(BUZZER, 300, 200);

      delay(1500);

      digitalWrite(RED_LED, LOW);

    }


    // -------------------------------------------------
    // VALID ATTENDANCE
    // -------------------------------------------------

    else {

      users[matchIndex].lastSeen = currentMillis;


      // Get current date/time from DS1307

      DateTime now = rtc.now();


      // ------------------------------------------------
      // GREEN LED + BUZZER
      // ------------------------------------------------

      digitalWrite(GREEN_LED, HIGH);

      tone(BUZZER, 1000, 200);


      // ------------------------------------------------
      // SHOW INFORMATION ON LCD
      // ------------------------------------------------

      showAttendance(users[matchIndex], now);


      // ------------------------------------------------
      // SEND CSV DATA TO PC
      // ------------------------------------------------

      Serial.print(users[matchIndex].name);
      Serial.print(",");

      Serial.print(users[matchIndex].admissionNo);
      Serial.print(",");

      Serial.print(users[matchIndex].rollNo);
      Serial.print(",");


      // RFID UID

      for (byte i = 0; i < 4; i++) {

        if (mfrc522.uid.uidByte[i] < 0x10) {
          Serial.print("0");
        }

        Serial.print(
          mfrc522.uid.uidByte[i],
          HEX
        );
      }


      Serial.print(",");


      // Date

      if (now.day() < 10) {
        Serial.print("0");
      }

      Serial.print(now.day());
      Serial.print("/");

      if (now.month() < 10) {
        Serial.print("0");
      }

      Serial.print(now.month());
      Serial.print("/");

      Serial.print(now.year());

      Serial.print(",");


      // Time

      if (now.hour() < 10) {
        Serial.print("0");
      }

      Serial.print(now.hour());
      Serial.print(":");

      if (now.minute() < 10) {
        Serial.print("0");
      }

      Serial.print(now.minute());
      Serial.print(":");

      if (now.second() < 10) {
        Serial.print("0");
      }

      Serial.println(now.second());


      // ------------------------------------------------
      // TURN OFF GREEN LED
      // ------------------------------------------------

      digitalWrite(GREEN_LED, LOW);
    }
  }


  // ===================================================
  // RETURN TO READY SCREEN
  // ===================================================

  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("Scan your card");


  // Stop RFID communication

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();


  delay(300);
}