"""
PC-side attendance logger.

Listens to the Arduino over USB serial. Each attendance scan arrives as:
    Name,AdmissionNo,RollNo,UID,Date,Time,Status
This script appends each line to attendance_log.csv.

Setup:
  pip install pyserial
  Set PORT below to your Arduino's port (Arduino IDE > Tools > Port)
  Run: python logger.py

Note: only one program can hold the serial port at a time — close the
Arduino IDE's Serial Monitor before running this script.
"""

import serial
import csv
import os

PORT = "COM3"
BAUD = 9600
LOGFILE = "attendance_log.csv"
HEADER = ["Name", "AdmissionNo", "RollNo", "UID", "Date", "Time", "Status"]

def main():
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"Listening on {PORT}... Press Ctrl+C to stop.")

    file_is_new = not os.path.exists(LOGFILE)

    with open(LOGFILE, "a", newline="") as f:
        writer = csv.writer(f)

        if file_is_new:
            writer.writerow(HEADER)
            f.flush()

        while True:
            try:
                line = ser.readline().decode("utf-8", errors="ignore").strip()

                if line and line.count(",") == 6:
                    row = line.split(",")
                    writer.writerow(row)
                    f.flush()
                    print(f"Logged: {row[0]} ({row[6]}) at {row[4]} {row[5]}")
                elif line:
                    print(f"[Arduino] {line}")

            except KeyboardInterrupt:
                print("\nStopped.")
                break
            except Exception as e:
                print("Error:", e)

if __name__ == "__main__":
    main()
