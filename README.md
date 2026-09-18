# RFID Attendance System

A simple RFID-based attendance system built with an Arduino UNO R3. Students tap a card on the reader instead of a manual roll call, and the system records each scan with the correct name, roll number, and timestamp.

## Features

- Reads MIFARE RFID cards/fobs using an RC522 reader
- Displays the student's name, admission number, roll number, and attendance status on a 16x2 LCD
- Flags scans as "On Time" or "Late" based on a configurable cutoff time
- Rejects unregistered cards with clear LED and buzzer feedback
- Real-time timestamps from a DS1307 RTC module, so no computer is needed to keep the correct time
- Students can be added or removed at any time using a dedicated Master Card, without reprogramming the Arduino
- Registered users are stored in EEPROM and persist through power loss
- Attendance records are sent over serial and can be logged to a CSV file on a connected computer

## Hardware Used

- Arduino UNO R3
- RC522 RFID reader module (also sold as "MFRC522" — same hardware)
- RFID cards and one keyfob
- 16x2 LCD (standard parallel, not I2C)
- DS1307 RTC module
- Green and red LEDs
- Passive buzzer
- 220Ω resistors
- 10kΩ potentiometer (for LCD contrast)
- Breadboard and jumper wires

## Setup

1. Wire the components according to the pin comments at the top of the main sketch.
2. Upload `test.ino` and use the Serial Monitor to read the UID of each card. Note these down.
3. Choose one card to act as the Master Card (a keyfob works well, since it's easy to keep separate from student cards). Set its UID as `MASTER_UID` in `attendance_system.ino`.
4. If the RTC is showing the wrong time, upload `SetRTCTime.ino` once with the correct date and time, then switch back to the main sketch.
5. Upload `attendance_system.ino`. This is the sketch that runs the full system.
6. To log attendance to a CSV file, install the required Python package (`pip install pyserial`) and run `logger.py` while the Arduino is connected.

## Using the Master Card

Tap the Master Card, then type `A` (add) or `D` (delete) into the Serial Monitor. Tap the target card next.

For adding a student, enter their details in this format when prompted:

```
John Smith,12345,07
```

This corresponds to Name, Admission Number, Roll Number. The record is written to EEPROM immediately and will still be there after a power cycle.

## Planned Improvements

- Add an ESP32 for WiFi connectivity, removing the need for a laptop to log attendance
- Email alerts for absences or late arrivals
- A basic web dashboard for viewing attendance data
