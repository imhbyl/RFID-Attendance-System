# RFID Attendance System
 
An RFID-based attendance system built around an Arduino UNO R3, with an ESP32 bridge for WiFi connectivity, Google Sheets as a backend, email alerts for late arrivals, and a web dashboard for viewing attendance data.
 
Students tap a card on the reader instead of a manual roll call. The system checks the card against a list of registered users, records the time using an onboard real-time clock, flags whether the student is on time or late, and syncs the record to a Google Sheet over WiFi. If a student arrives late, their parent gets an automatic email.
 
## How it works
 
```
RFID card --> RC522 reader --> Arduino UNO --> ESP32 --> Google Sheets
                                     |                        |
                              LCD, LEDs, buzzer          Email alerts
                                     |
                                logger.py (local CSV backup)
```
 
The Arduino UNO handles the actual attendance logic: reading cards, checking them against stored users, showing feedback on the LCD, and keeping time. It sends each valid scan over two channels at once — over USB to a connected computer (for local CSV logging), and over a second wired serial connection to the ESP32, which pushes the record to Google Sheets over WiFi and can send email alerts.
 
## Hardware
 
- Arduino UNO R3
- RC522 RFID reader module (also sold as "MFRC522" — same hardware)
- ESP32 dev board
- RFID cards and one keyfob (used as the admin/Master Card)
- 16x2 LCD (standard parallel, not I2C)
- DS1307 RTC module
- Green and red LEDs
- Passive buzzer
- 220Ω resistors (x2 for the LEDs)
- 10kΩ potentiometer (LCD contrast)
- 1kΩ and 2kΩ resistors (voltage divider between UNO and ESP32)
- Breadboard and jumper wires
## Wiring
 
**RC522 → Arduino UNO**
 
| RC522 pin | Arduino pin |
|---|---|
| RST | D9 |
| SDA | D10 |
| MOSI | D11 |
| MISO | D12 |
| SCK | D13 |
| 3.3V | 3.3V |
| GND | GND |
 
The RC522 must be powered from 3.3V, not 5V — connecting it to 5V will damage the module.
 
**LCD (parallel) → Arduino UNO**
 
| LCD pin | Arduino pin |
|---|---|
| RS | D8 |
| E | D7 |
| D4 | D6 |
| D5 | D5 |
| D6 | D4 |
| D7 | D3 |
| RW, VSS | GND |
| VDD | 5V |
| VO | Wiper of the 10kΩ potentiometer (other two legs to 5V and GND) |
 
**LEDs and buzzer**
 
| Component | Arduino pin |
|---|---|
| Green LED (through 220Ω) | A0 |
| Red LED (through 220Ω) | A1 |
| Buzzer | A2 |
 
**Arduino UNO ↔ ESP32**
 
The UNO runs at 5V logic and the ESP32 only tolerates 3.3V, so the connection carrying data from the UNO to the ESP32 needs a voltage divider.
 
- UNO A3 → 1kΩ resistor → junction → ESP32 RX2 (GPIO16)
- That same junction → 2kΩ resistor → GND
- ESP32 TX2 (GPIO17) → UNO D2 (no divider needed this direction)
- UNO GND → ESP32 GND (shared ground, required)
## Software setup
 
### Arduino IDE
 
1. Install the `MFRC522` library and the `RTClib` library (Library Manager).
2. Install ESP32 board support: File > Preferences > Additional Board Manager URLs, add:
   `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   then install "esp32" from Boards Manager.
### Setup order
 
1. Upload `Step1_FindCardUID.ino` to the UNO and note the UID of each card, including the one you'll use as the Master Card.
2. Set `MASTER_UID` in `RFID_Attendance_System.ino` to your Master Card's UID.
3. If the RTC shows the wrong time, upload `SetRTCTime.ino` once with the correct date/time, then switch back.
4. Set up the Google Sheet and Apps Script (below), then set `WIFI_SSID`, `WIFI_PASSWORD`, and `WEB_APP_URL` in `ESP32_WiFiBridge.ino` and upload it to the ESP32 by itself to confirm it connects.
5. Wire the UNO and ESP32 together as described above.
6. Upload `RFID_Attendance_System.ino` to the UNO.
7. Run `logger.py` on a computer connected to the UNO for local CSV backup (optional, since the ESP32 also logs to Google Sheets independently).
### Google Sheet + Apps Script
 
1. Create a Google Sheet with a header row: `Name, AdmissionNo, RollNo, Class, Email, Date, Time, Status, EmailResult`
2. Open Extensions > Apps Script, paste in `GoogleAppsScript.gs`, and set `FALLBACK_EMAIL` to your own address.
3. Deploy > New Deployment > Web app. Set "Execute as" to yourself and "Who has access" to "Anyone". Copy the resulting URL into `WEB_APP_URL` in the ESP32 sketch.
4. Any time the script is edited afterward, redeploy with Deploy > Manage Deployments > edit > New version — saving alone doesn't update a live deployment.
### Dashboard
 
`attendance_dashboard.html` can be used two ways:
 
- Opened directly (or via Claude's artifact preview) and loaded manually with a CSV file exported from `logger.py`.
- Hosted on GitHub Pages, where it can also connect live to the Google Sheet: File > Share > Publish to web in Sheets, choose CSV format, and paste the resulting URL into the dashboard's "Connect" box. It refreshes automatically every 30 seconds. This live-fetch mode only works once hosted outside Claude's own preview, due to browser restrictions in that environment.
## Using the Master Card
 
Tap the Master Card (the keyfob), then in the Serial Monitor (or `logger.py`, which can also send commands) type `A` to add a student or `D` to delete one, then tap the target card.
 
When adding, enter details in this format:
 
```
Name,AdmissionNo,RollNo,Class,ParentEmail
```
 
Example:
 
```
John Smith,12345,07,9A,parent@example.com
```
 
This is saved to EEPROM immediately and survives a power cycle.
 
## logger.py
 
Run this on a computer connected to the Arduino via USB. It logs every attendance record to `attendance_log.csv` and also lets you type Master Card commands directly into the same terminal window — no need to have the Arduino IDE's Serial Monitor open at all once this is running.
 
```
pip install pyserial
python logger.py
```
 
Set the `PORT` variable at the top of the file to match your Arduino's COM port first.
 
## Notes
 
- The Arduino UNO has only 2KB of RAM, so name, admission number, roll number, class, and email fields all have fixed, limited lengths to stay within memory. If the struct ever changes size, `EEPROM_LAYOUT_VERSION` in the sketch needs to be incremented — the code detects a mismatch automatically and clears old, incompatible data rather than reading it as garbage.
- If the RFID reader stops responding, unplugging and reconnecting the Arduino usually resolves it. This is a known quirk of the RC522 module under repeated rapid reads.
- Only one program can access the Arduino's serial port at a time — close the Serial Monitor before running `logger.py`, and vice versa.
- Google Apps Script Web Apps sometimes respond with HTTP 200 even when the script fails internally. The dashboard writes any email-sending failure directly into the sheet's `EmailResult` column, so failures are visible without checking the Apps Script execution log.
## Files
 
| File | Runs on | Purpose |
|---|---|---|
| `RFID_Attendance_System.ino` | Arduino UNO | Main attendance logic |
| `Step1_FindCardUID.ino` | Arduino UNO | One-time use, to find card UIDs |
| `SetRTCTime.ino` | Arduino UNO | One-time use, to set the RTC clock |
| `ESP32_WiFiBridge.ino` | ESP32 | Forwards attendance data to Google Sheets |
| `GoogleAppsScript.gs` | Google Apps Script | Receives data, logs it, sends email alerts |
| `logger.py` | Computer | Local CSV backup and Master Card admin console |
| `attendance_dashboard.html` | Browser | Attendance viewer and analytics dashboard |
 
