/*
  RFID ATTENDANCE SYSTEM - UNO
  =============================

  UNO:
    MFRC522 RFID
    DS1307 RTC
    16x2 parallel LCD
    Green LED
    Red LED
    Buzzer
    EEPROM student database
    ESP32 serial connection

  RFID ONLY
  No PIN
  No keypad

  Attendance data sent to ESP32:

  Name,AdmissionNo,RollNo,Class,ParentEmail,UID,Date,Time,Status
*/

#include <SPI.h>
#include <SoftwareSerial.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <string.h>
#include <ctype.h>

// ======================================================
// RFID
// ======================================================

#define SS_PIN 10
#define RST_PIN 9

MFRC522 mfrc522(SS_PIN, RST_PIN);

// ======================================================
// ESP32 SERIAL
// ======================================================

#define UNO_RX 2
#define UNO_TX A3

SoftwareSerial espSerial(UNO_RX, UNO_TX);

// ======================================================
// RTC + LCD
// ======================================================

RTC_DS1307 rtc;

LiquidCrystal lcd(
  8,  // RS
  7,  // E
  6,  // D4
  5,  // D5
  4,  // D6
  3   // D7
);

// ======================================================
// OUTPUTS
// ======================================================

#define GREEN_LED A0
#define RED_LED   A1
#define BUZZER    A2

// ======================================================
// MASTER CARD
// ======================================================

byte MASTER_UID[4] = {
  0xAE,
  0x52,
  0x99,
  0x04
};

// ======================================================
// LATE TIME
// ======================================================

#define LATE_HOUR   7
#define LATE_MINUTE 30

// ======================================================
// EEPROM DATABASE
// ======================================================

struct User {

  byte uid[4];

  char name[13];

  char admissionNo[6];

  char rollNo[3];

  char className[5];

  char parentEmail[36];
};

#define MAX_USERS 4

/*
  Changed because the old EEPROM structure contained
  the PIN field.
*/
#define EEPROM_LAYOUT_VERSION 6

#define EEPROM_VERSION_ADDR 0
#define EEPROM_COUNT_ADDR   1
#define EEPROM_USERS_START  2

#define USER_RECORD_SIZE sizeof(User)

User currentUser;

int currentUserIndex = -1;

byte userCount = 0;

// ======================================================
// COOLDOWN
// ======================================================

unsigned long lastSeen[MAX_USERS];

#define COOLDOWN_MS 10000UL

// ======================================================
// FUNCTION DECLARATIONS
// ======================================================

bool sameUID(byte* a, byte* b);

int findUser(byte* uid);

int findUserByAdmission(const char* admission);

void loadUser(int index);

void saveUser(int index, User &u);

void deleteUser(int index);

int readSerialLine(char* buf, int maxLen);

void enrollNewUser(byte* uid);

void showAttendance(
  User &user,
  DateTime now,
  bool isLate
);

void sendAttendanceToESP32(
  User &user,
  byte* uid,
  DateTime now,
  bool isLate
);

// ======================================================
// LOAD USER
// ======================================================

void loadUser(int index) {

  EEPROM.get(
    EEPROM_USERS_START +
    ((long)index * USER_RECORD_SIZE),
    currentUser
  );

  currentUserIndex = index;
}

// ======================================================
// SAVE USER
// ======================================================

void saveUser(int index, User &u) {

  EEPROM.put(
    EEPROM_USERS_START +
    ((long)index * USER_RECORD_SIZE),
    u
  );
}

// ======================================================
// COMPARE UID
// ======================================================

bool sameUID(byte* a, byte* b) {

  for (byte i = 0; i < 4; i++) {

    if (a[i] != b[i])
      return false;
  }

  return true;
}

// ======================================================
// FIND USER BY UID
// ======================================================

int findUser(byte* uid) {

  User temp;

  for (byte i = 0; i < userCount; i++) {

    EEPROM.get(
      EEPROM_USERS_START +
      ((long)i * USER_RECORD_SIZE),
      temp
    );

    if (sameUID(uid, temp.uid)) {

      return i;
    }
  }

  return -1;
}

// ======================================================
// FIND USER BY ADMISSION NUMBER
// ======================================================

int findUserByAdmission(
  const char* admission
) {

  User temp;

  for (byte i = 0; i < userCount; i++) {

    EEPROM.get(
      EEPROM_USERS_START +
      ((long)i * USER_RECORD_SIZE),
      temp
    );

    if (
      strcmp(
        temp.admissionNo,
        admission
      ) == 0
    ) {

      return i;
    }
  }

  return -1;
}

// ======================================================
// DELETE USER
// ======================================================

void deleteUser(int index) {

  User temp;

  for (
    int i = index;
    i < userCount - 1;
    i++
  ) {

    EEPROM.get(
      EEPROM_USERS_START +
      ((long)(i + 1) * USER_RECORD_SIZE),
      temp
    );

    EEPROM.put(
      EEPROM_USERS_START +
      ((long)i * USER_RECORD_SIZE),
      temp
    );

    lastSeen[i] = lastSeen[i + 1];
  }

  if (userCount > 0)
    userCount--;

  EEPROM.update(
    EEPROM_COUNT_ADDR,
    userCount
  );

  currentUserIndex = -1;
}

// ======================================================
// READ SERIAL LINE
// ======================================================

int readSerialLine(
  char* buf,
  int maxLen
) {

  int len =
    Serial.readBytesUntil(
      '\n',
      buf,
      maxLen - 1
    );

  buf[len] = '\0';

  while (
    len > 0 &&
    (
      buf[len - 1] == '\r' ||
      buf[len - 1] == ' '
    )
  ) {

    buf[--len] = '\0';
  }

  return len;
}

// ======================================================
// REGISTER NEW STUDENT
// ======================================================

void enrollNewUser(byte* uid) {

  if (userCount >= MAX_USERS) {

    lcd.clear();
    lcd.print(F("Storage FULL"));

    Serial.println(
      F("ERROR: No space for more users.")
    );

    delay(2000);

    return;
  }

  if (findUser(uid) != -1) {

    lcd.clear();
    lcd.print(F("Already exists"));

    Serial.println(
      F("ERROR: Card already registered.")
    );

    delay(1500);

    return;
  }

  lcd.clear();
  lcd.print(F("Check Serial"));

  lcd.setCursor(0, 1);
  lcd.print(F("Monitor now"));

  Serial.println();
  Serial.println(
    F("------------------------------")
  );

  Serial.println(
    F("NEW STUDENT REGISTRATION")
  );

  Serial.println(
    F("Enter:")
  );

  Serial.println(
    F("Name,AdmissionNo,RollNo,Class,ParentEmail")
  );

  Serial.println(
    F("Example:")
  );

  Serial.println(
    F("Hubayl,69254,02,12D,parent@gmail.com")
  );

  Serial.println(
    F("------------------------------")
  );

  while (!Serial.available()) {

    delay(50);
  }

  char inputBuf[75];

  readSerialLine(
    inputBuf,
    sizeof(inputBuf)
  );

  char* namePart =
    strtok(inputBuf, ",");

  char* admPart =
    strtok(NULL, ",");

  char* rollPart =
    strtok(NULL, ",");

  char* classPart =
    strtok(NULL, ",");

  char* emailPart =
    strtok(NULL, ",");

  if (
    !namePart ||
    !admPart ||
    !rollPart ||
    !classPart ||
    !emailPart
  ) {

    Serial.println(
      F("ERROR: Wrong format.")
    );

    lcd.clear();
    lcd.print(F("Enroll Failed"));

    delay(1500);

    return;
  }

  // Check duplicate admission number

  if (
    findUserByAdmission(admPart) != -1
  ) {

    Serial.println(
      F("ERROR: Admission number exists.")
    );

    lcd.clear();
    lcd.print(F("Adm No Exists"));

    delay(1800);

    return;
  }

  User u;

  memset(
    &u,
    0,
    sizeof(User)
  );

  memcpy(
    u.uid,
    uid,
    4
  );

  strncpy(
    u.name,
    namePart,
    sizeof(u.name) - 1
  );

  strncpy(
    u.admissionNo,
    admPart,
    sizeof(u.admissionNo) - 1
  );

  strncpy(
    u.rollNo,
    rollPart,
    sizeof(u.rollNo) - 1
  );

  strncpy(
    u.className,
    classPart,
    sizeof(u.className) - 1
  );

  strncpy(
    u.parentEmail,
    emailPart,
    sizeof(u.parentEmail) - 1
  );

  saveUser(
    userCount,
    u
  );

  lastSeen[userCount] = 0;

  userCount++;

  EEPROM.update(
    EEPROM_COUNT_ADDR,
    userCount
  );

  // ==================================================
  // DISPLAY RESULT
  // ==================================================

  Serial.print(
    F("Enrolled: ")
  );

  Serial.println(
    u.name
  );

  Serial.print(
    F("Admission No: ")
  );

  Serial.println(
    u.admissionNo
  );

  Serial.print(
    F("UID saved: ")
  );

  for (byte i = 0; i < 4; i++) {

    if (u.uid[i] < 0x10)
      Serial.print('0');

    Serial.print(
      u.uid[i],
      HEX
    );

    Serial.print(' ');
  }

  Serial.println();

  lcd.clear();
  lcd.print(F("Enrolled:"));

  lcd.setCursor(0, 1);
  lcd.print(u.name);

  delay(2000);
}

// ======================================================
// SHOW ATTENDANCE
// ======================================================

void showAttendance(
  User &user,
  DateTime now,
  bool isLate
) {

  lcd.clear();

  lcd.print(F("Welcome"));

  lcd.setCursor(0, 1);

  lcd.print(user.name);

  delay(1200);

  lcd.clear();

  lcd.print(F("Adm:"));

  lcd.print(user.admissionNo);

  lcd.setCursor(0, 1);

  lcd.print(F("Roll:"));

  lcd.print(user.rollNo);

  lcd.print(' ');

  lcd.print(user.className);

  delay(1200);

  lcd.clear();

  if (isLate)
    lcd.print(F("Status: LATE"));
  else
    lcd.print(F("Status: ON TIME"));

  lcd.setCursor(0, 1);

  if (now.hour() < 10)
    lcd.print('0');

  lcd.print(now.hour());

  lcd.print(':');

  if (now.minute() < 10)
    lcd.print('0');

  lcd.print(now.minute());

  delay(1500);
}

// ======================================================
// SEND ATTENDANCE TO ESP32
// ======================================================

void sendAttendanceToESP32(
  User &user,
  byte* uid,
  DateTime now,
  bool isLate
) {

  // Name
  espSerial.print(user.name);
  espSerial.print(',');

  // Admission
  espSerial.print(user.admissionNo);
  espSerial.print(',');

  // Roll
  espSerial.print(user.rollNo);
  espSerial.print(',');

  // Class
  espSerial.print(user.className);
  espSerial.print(',');

  // Parent Email
  espSerial.print(user.parentEmail);
  espSerial.print(',');

  // UID
  for (byte i = 0; i < 4; i++) {

    if (uid[i] < 0x10)
      espSerial.print('0');

    espSerial.print(
      uid[i],
      HEX
    );
  }

  espSerial.print(',');

  // Date

  if (now.day() < 10)
    espSerial.print('0');

  espSerial.print(now.day());

  espSerial.print('/');

  if (now.month() < 10)
    espSerial.print('0');

  espSerial.print(now.month());

  espSerial.print('/');

  espSerial.print(now.year());

  espSerial.print(',');

  // Time

  if (now.hour() < 10)
    espSerial.print('0');

  espSerial.print(now.hour());

  espSerial.print(':');

  if (now.minute() < 10)
    espSerial.print('0');

  espSerial.print(now.minute());

  espSerial.print(':');

  if (now.second() < 10)
    espSerial.print('0');

  espSerial.print(now.second());

  espSerial.print(',');

  // Status

  if (isLate)
    espSerial.println(
      F("LATE")
    );
  else
    espSerial.println(
      F("ON TIME")
    );
}

// ======================================================
// SETUP
// ======================================================

void setup() {

  Serial.begin(9600);

  espSerial.begin(9600);

  // RFID
  SPI.begin();

  mfrc522.PCD_Init();

  // RTC
  Wire.begin();

  // LCD
  lcd.begin(
    16,
    2
  );

  lcd.clear();

  lcd.print(
    F("Starting...")
  );

  delay(1000);

  // ==================================================
  // RTC CHECK
  // ==================================================

  if (!rtc.begin()) {

    lcd.clear();

    lcd.print(
      F("RTC ERROR!")
    );

    Serial.println(
      F("ERROR: DS1307 not found.")
    );

    while (1);
  }

  if (!rtc.isrunning()) {

    lcd.clear();

    lcd.print(
      F("RTC NOT RUNNING")
    );

    Serial.println(
      F("WARNING: RTC is not running.")
    );

    delay(2000);
  }

  // ==================================================
  // OUTPUTS
  // ==================================================

  pinMode(
    GREEN_LED,
    OUTPUT
  );

  pinMode(
    RED_LED,
    OUTPUT
  );

  pinMode(
    BUZZER,
    OUTPUT
  );

  digitalWrite(
    GREEN_LED,
    LOW
  );

  digitalWrite(
    RED_LED,
    LOW
  );

  digitalWrite(
    BUZZER,
    LOW
  );

  // ==================================================
  // EEPROM
  // ==================================================

  byte storedVersion =
    EEPROM.read(
      EEPROM_VERSION_ADDR
    );

  if (
    storedVersion !=
    EEPROM_LAYOUT_VERSION
  ) {

    userCount = 0;

    EEPROM.update(
      EEPROM_VERSION_ADDR,
      EEPROM_LAYOUT_VERSION
    );

    EEPROM.update(
      EEPROM_COUNT_ADDR,
      0
    );

    Serial.println(
      F("EEPROM layout reset.")
    );

  } else {

    userCount =
      EEPROM.read(
        EEPROM_COUNT_ADDR
      );

    if (userCount > MAX_USERS) {

      userCount = 0;

      EEPROM.update(
        EEPROM_COUNT_ADDR,
        0
      );
    }
  }

  for (
    byte i = 0;
    i < MAX_USERS;
    i++
  ) {

    lastSeen[i] = 0;
  }

  Serial.print(
    F("Loaded ")
  );

  Serial.print(
    userCount
  );

  Serial.println(
    F(" users from EEPROM.")
  );

  // ==================================================
  // RESET WINDOW
  // ==================================================

  Serial.println(
    F("Type RESET within 10 seconds to erase all users.")
  );

  unsigned long resetWindowStart =
    millis();

  while (
    millis() - resetWindowStart <
    10000UL
  ) {

    if (Serial.available()) {

      char cmdBuf[8];

      readSerialLine(
        cmdBuf,
        sizeof(cmdBuf)
      );

      if (
        strcasecmp(
          cmdBuf,
          "RESET"
        ) == 0
      ) {

        userCount = 0;

        EEPROM.update(
          EEPROM_VERSION_ADDR,
          EEPROM_LAYOUT_VERSION
        );

        EEPROM.update(
          EEPROM_COUNT_ADDR,
          0
        );

        Serial.println(
          F("All users erased.")
        );

        lcd.clear();

        lcd.print(
          F("All users wiped")
        );

        delay(1500);
      }

      break;
    }
  }

  // ==================================================
  // READY
  // ==================================================

  lcd.clear();

  lcd.print(
    F("Attendance")
  );

  lcd.setCursor(
    0,
    1
  );

  lcd.print(
    F("System Ready")
  );

  delay(1500);

  lcd.clear();

  lcd.print(
    F("Scan your card")
  );
}

// ======================================================
// LOOP
// ======================================================

void loop() {

  // --------------------------------------------------
  // Wait for RFID card
  // --------------------------------------------------

  if (
    !mfrc522.PICC_IsNewCardPresent()
  )
    return;

  if (
    !mfrc522.PICC_ReadCardSerial()
  )
    return;

  // --------------------------------------------------
  // Only 4-byte UID supported
  // --------------------------------------------------

  if (
    mfrc522.uid.size != 4
  ) {

    lcd.clear();

    lcd.print(
      F("Unsupported card")
    );

    digitalWrite(
      RED_LED,
      HIGH
    );

    tone(
      BUZZER,
      300,
      400
    );

    delay(1200);

    digitalWrite(
      RED_LED,
      LOW
    );

    lcd.clear();

    lcd.print(
      F("Scan your card")
    );

    mfrc522.PICC_HaltA();

    return;
  }

  // ==================================================
  // MASTER CARD
  // ==================================================

  if (
    sameUID(
      mfrc522.uid.uidByte,
      MASTER_UID
    )
  ) {

    lcd.clear();

    lcd.print(
      F("MASTER CARD")
    );

    lcd.setCursor(
      0,
      1
    );

    lcd.print(
      F("Check Serial")
    );

    tone(
      BUZZER,
      800,
      150
    );

    mfrc522.PICC_HaltA();

    mfrc522.PCD_Init();

    delay(100);

    Serial.println(
      F("Type A to Add or D to Delete, then Enter")
    );

    unsigned long waitStart =
      millis();

    while (!Serial.available()) {

      if (
        millis() - waitStart >
        15000UL
      ) {

        lcd.clear();

        lcd.print(
          F("Timeout")
        );

        delay(1200);

        lcd.clear();

        lcd.print(
          F("Scan your card")
        );

        return;
      }

      delay(50);
    }

    char choiceBuf[5];

    readSerialLine(
      choiceBuf,
      sizeof(choiceBuf)
    );

    char choice =
      toupper(
        choiceBuf[0]
      );

    // =================================================
    // ADD USER
    // =================================================

    if (choice == 'A') {

      lcd.clear();

      lcd.print(
        F("Tap new card...")
      );

      mfrc522.PCD_Init();

      delay(100);

      waitStart =
        millis();

      while (true) {

        if (
          mfrc522.PICC_IsNewCardPresent() &&
          mfrc522.PICC_ReadCardSerial()
        ) {

          if (
            mfrc522.uid.size != 4
          ) {

            lcd.clear();

            lcd.print(
              F("Unsupported")
            );

            delay(1200);

            break;
          }

          byte newUID[4];

          memcpy(
            newUID,
            mfrc522.uid.uidByte,
            4
          );

          enrollNewUser(
            newUID
          );

          mfrc522.PICC_HaltA();

          mfrc522.PCD_Init();

          delay(100);

          break;
        }

        if (
          millis() - waitStart >
          15000UL
        ) {

          lcd.clear();

          lcd.print(
            F("Enroll Timeout")
          );

          delay(1200);

          break;
        }
      }
    }

    // =================================================
    // DELETE USER
    // =================================================

    else if (choice == 'D') {

      lcd.clear();

      lcd.print(
        F("Tap card to del")
      );

      mfrc522.PCD_Init();

      delay(100);

      waitStart =
        millis();

      while (true) {

        if (
          mfrc522.PICC_IsNewCardPresent() &&
          mfrc522.PICC_ReadCardSerial()
        ) {

          int idx =
            findUser(
              mfrc522.uid.uidByte
            );

          if (idx == -1) {

            lcd.clear();

            lcd.print(
              F("Not Found")
            );

            Serial.println(
              F("Card isn't registered.")
            );

          } else {

            loadUser(idx);

            char removedName[13];

            strncpy(
              removedName,
              currentUser.name,
              sizeof(removedName)
            );

            removedName[
              sizeof(removedName) - 1
            ] = '\0';

            deleteUser(idx);

            lcd.clear();

            lcd.print(
              F("Deleted:")
            );

            lcd.setCursor(
              0,
              1
            );

            lcd.print(
              removedName
            );

            Serial.print(
              F("Deleted: ")
            );

            Serial.println(
              removedName
            );
          }

          delay(1500);

          mfrc522.PICC_HaltA();

          mfrc522.PCD_Init();

          delay(100);

          break;
        }

        if (
          millis() - waitStart >
          15000UL
        ) {

          lcd.clear();

          lcd.print(
            F("Delete Timeout")
          );

          delay(1200);

          break;
        }
      }
    }

    else {

      lcd.clear();

      lcd.print(
        F("Unknown choice")
      );

      delay(1200);
    }

    lcd.clear();

    lcd.print(
      F("Scan your card")
    );

    return;
  }

  // ==================================================
  // NORMAL USER
  // ==================================================

  int matchIndex =
    findUser(
      mfrc522.uid.uidByte
    );

  // --------------------------------------------------
  // Not registered
  // --------------------------------------------------

  if (matchIndex == -1) {

    Serial.print(
      F("UID scanned: ")
    );

    for (byte i = 0; i < 4; i++) {

      if (
        mfrc522.uid.uidByte[i] <
        0x10
      )
        Serial.print('0');

      Serial.print(
        mfrc522.uid.uidByte[i],
        HEX
      );

      Serial.print(' ');
    }

    Serial.println();

    lcd.clear();

    lcd.print(
      F("Not Registered")
    );

    lcd.setCursor(
      0,
      1
    );

    lcd.print(
      F("Access Denied")
    );

    digitalWrite(
      RED_LED,
      HIGH
    );

    tone(
      BUZZER,
      300,
      500
    );

    delay(1200);

    digitalWrite(
      RED_LED,
      LOW
    );
  }

  // --------------------------------------------------
  // Registered user
  // --------------------------------------------------

  else {

    unsigned long currentMillis =
      millis();

    loadUser(
      matchIndex
    );

    // Cooldown

    if (
      lastSeen[matchIndex] != 0 &&
      currentMillis -
      lastSeen[matchIndex] <
      COOLDOWN_MS
    ) {

      lcd.clear();

      lcd.print(
        currentUser.name
      );

      lcd.setCursor(
        0,
        1
      );

      lcd.print(
        F("Already Marked")
      );

      digitalWrite(
        RED_LED,
        HIGH
      );

      tone(
        BUZZER,
        300,
        200
      );

      delay(1200);

      digitalWrite(
        RED_LED,
        LOW
      );
    }

    // ------------------------------------------------
    // Mark attendance immediately
    // ------------------------------------------------

    else {

      lastSeen[matchIndex] =
        currentMillis;

      DateTime now =
        rtc.now();

      bool isLate =
        (
          now.hour() > LATE_HOUR
        ) ||
        (
          now.hour() == LATE_HOUR &&
          now.minute() >= LATE_MINUTE
        );

      // Green LED
      digitalWrite(
        GREEN_LED,
        HIGH
      );

      // Buzzer
      tone(
        BUZZER,
        1000,
        200
      );

      // LCD
      showAttendance(
        currentUser,
        now,
        isLate
      );

      // USB serial log

      Serial.print(
        currentUser.name
      );

      Serial.print(',');

      Serial.print(
        currentUser.admissionNo
      );

      Serial.print(',');

      Serial.print(
        currentUser.rollNo
      );

      Serial.print(',');

      Serial.print(
        currentUser.className
      );

      Serial.print(',');

      Serial.print(
        currentUser.parentEmail
      );

      Serial.print(',');

      for (byte i = 0; i < 4; i++) {

        if (
          mfrc522.uid.uidByte[i] <
          0x10
        )
          Serial.print('0');

        Serial.print(
          mfrc522.uid.uidByte[i],
          HEX
        );
      }

      Serial.print(',');

      // Date

      if (now.day() < 10)
        Serial.print('0');

      Serial.print(
        now.day()
      );

      Serial.print('/');

      if (now.month() < 10)
        Serial.print('0');

      Serial.print(
        now.month()
      );

      Serial.print('/');

      Serial.print(
        now.year()
      );

      Serial.print(',');

      // Time

      if (now.hour() < 10)
        Serial.print('0');

      Serial.print(
        now.hour()
      );

      Serial.print(':');

      if (now.minute() < 10)
        Serial.print('0');

      Serial.print(
        now.minute()
      );

      Serial.print(':');

      if (now.second() < 10)
        Serial.print('0');

      Serial.print(
        now.second()
      );

      Serial.print(',');

      // Status

      if (isLate)
        Serial.println(
          F("LATE")
        );
      else
        Serial.println(
          F("ON TIME")
        );

      // Send to ESP32

      sendAttendanceToESP32(
        currentUser,
        mfrc522.uid.uidByte,
        now,
        isLate
      );

      digitalWrite(
        GREEN_LED,
        LOW
      );
    }
  }

  // ==================================================
  // RESET RFID
  // ==================================================

  lcd.clear();

  lcd.print(
    F("Scan your card")
  );

  mfrc522.PICC_HaltA();

  mfrc522.PCD_Init();

  delay(300);
}