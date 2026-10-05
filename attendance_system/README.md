# RFID Attendance Monitoring System

An RFID-based attendance system built for EDUEX. Students tap an RFID card, the system records their attendance, identifies late arrivals, updates a live Google Sheet, and can send a late-arrival email alert to the registered parent email.

The system also stores every attendance scan on a microSD card. This provides a local backup if Wi-Fi or the cloud service is unavailable.

## Features

- RFID card-based attendance using an RC522 reader
- LCD, LEDs, and buzzer feedback for successful or rejected scans
- Date and time from a DS1307 RTC module
- Late-arrival detection
- Master-card enrollment and deletion of student cards
- Student details saved in Arduino EEPROM
- Wi-Fi upload to Google Sheets through an ESP32
- Parent email alerts for late attendance
- Attendance dashboard with filters and attendance percentage
- Local microSD-card backup for every scan

## How it works

```text
RFID card
   ↓
Arduino Uno: reads the card and checks the student
   ↓
LCD / LED / buzzer feedback
   ↓
ESP32: uploads the attendance record through Wi-Fi
   ↓                         ↘
Google Sheets + email alerts  microSD backup
```

Each attendance record uses this format:

```text
Name,AdmissionNo,RollNo,Class,ParentEmail,UID,Date,Time,Status
```

The ESP32 saves the record to the SD card before attempting the Google Sheets upload. The card contains:

- `attendance_backup.csv` — every scan made by the system
- `sync_status.csv` — whether the cloud upload was `SYNCED` or remained pending because Wi-Fi was unavailable

> Current version: the SD card is a reliable local backup and records cloud status. Automatic uploading of previously pending SD records after Wi-Fi returns is a planned improvement.

## Hardware used

| Part | Purpose |
|---|---|
| Arduino Uno R3 | Main attendance controller |
| ESP32 DevKit V1 | Wi-Fi and Google Sheets connection |
| RC522 RFID reader and RFID cards | Student identification |
| LCD1602 parallel display | Displays student and attendance information |
| DS1307 RTC module | Keeps date and time |
| LEDs and passive buzzer | Visual and audio feedback |
| 10k potentiometer | LCD contrast control |
| Resistors | LED protection and Uno-to-ESP32 voltage divider |
| microSD card module and 2 GB card | Local attendance backup |
| Breadboard and jumper wires | Circuit connections |

## Important wiring

### Arduino Uno to ESP32

The Uno uses 5V logic while the ESP32 uses 3.3V logic. A voltage divider is required on the Uno-to-ESP32 signal.

```text
Uno A3 ── 1kΩ resistor ──+── ESP32 GPIO16 / RX2
                         |
                       2kΩ resistor
                         |
                        GND

ESP32 GPIO17 / TX2 ───────── Uno D2
ESP32 GND ─────────────────── Uno GND
```

Do not connect Uno 5V directly to an ESP32 GPIO pin.

## Software setup

### Arduino Uno sketch

Upload the Uno attendance sketch first. It handles RFID reading, student storage, RTC time, display messages, LEDs, buzzer feedback, and sending attendance lines to the ESP32.

### ESP32 sketch

Before uploading the ESP32 Wi-Fi bridge sketch:

1. Enter your Wi-Fi name and password.
2. Enter your deployed Google Apps Script Web App URL.
3. Insert a FAT32-formatted SD card.
4. Select the correct ESP32 board and port in Arduino IDE.
5. Open Serial Monitor at `115200` baud after upload.

Expected successful output:

```text
SD card ready.
Received from Uno: ...
Saved to SD card.
Cloud result: SYNCED
```

For an offline test, temporarily comment out `startWiFi();` in the ESP32 `setup()` function. A scan should show:

```text
Saved to SD card.
Cloud result: PENDING_OFFLINE
```

Power off the ESP32 before removing the SD card. Open `attendance_backup.csv` on a computer to confirm the saved records.

## Arduino IDE installations

Install the ESP32 board package through **Tools → Board → Boards Manager**:

```text
esp32 by Espressif Systems
```

Install these libraries through **Sketch → Include Library → Manage Libraries**:

```text
MFRC522 by GithubCommunity / Miguel Balboa
RTClib by Adafruit
```

These are normally included with Arduino IDE or the ESP32 board package:

```text
LiquidCrystal
EEPROM
SoftwareSerial
SPI
SD
WiFi
HTTPClient
HardwareSerial
```

## Demonstration flow

1. Power the Uno and ESP32.
2. Wait for the ESP32 to connect to Wi-Fi.
3. Tap a registered RFID card.
4. Show the LCD feedback and LED/buzzer response.
5. Show the new row in Google Sheets and the dashboard.
6. Show `attendance_backup.csv` on the SD card as the offline safety feature.

## Future improvements

- Automatically re-upload pending SD records when Wi-Fi returns
- Add a more compact enclosure and labelled wiring
- Add a secure administrator page for student management
- Add a camera-based second check with appropriate consent and privacy controls
- Add analytics for absent students and class attendance trends

## Safety and privacy

Keep Wi-Fi passwords, Google Apps Script URLs, and student/parent data out of public GitHub repositories. Use test data during demonstrations whenever possible.
