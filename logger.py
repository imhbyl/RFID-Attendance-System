"""
PC-side attendance logger + admin console.

Listens to the Arduino over USB serial. Each attendance scan arrives as:
    Name,AdmissionNo,RollNo,Class,UID,Date,Time,Status
This script appends each line to attendance_log.csv.

It also lets you talk BACK to the Arduino from this same window — so when
you tap the Master Card and it asks "Type A to Add or D to Delete", just
type your answer here and press Enter. Same for the enrollment details
(Name,AdmissionNo,RollNo,Class) and the startup RESET command. You no
longer need the Arduino IDE's Serial Monitor open at all once this is
running.

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
import threading

PORT = "COM3"
BAUD = 9600
LOGFILE = "attendance_log.csv"
HEADER = ["Name", "AdmissionNo", "RollNo", "Class", "UID", "Date", "Time", "Status"]


def listen_loop(ser, writer, f):
    """Runs in the background, constantly reading whatever the Arduino sends."""
    while True:
        try:
            line = ser.readline().decode("utf-8", errors="ignore").strip()

            if line and line.count(",") == 7:
                row = line.split(",")
                writer.writerow(row)
                f.flush()
                print(f"Logged: {row[0]} ({row[7]}) at {row[5]} {row[6]}")
            elif line:
                # Prompts, boot messages, "Enrolled: ...", etc. show up here
                print(f"[Arduino] {line}")

        except Exception as e:
            print("Error:", e)


def main():
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"Connected on {PORT}.")
    print("Whenever the Arduino asks a question (like A/D, or Name,Adm,Roll,Class),")
    print("just type your answer here and press Enter.")
    print("Press Ctrl+C to stop.\n")

    file_is_new = not os.path.exists(LOGFILE)

    with open(LOGFILE, "a", newline="") as f:
        writer = csv.writer(f)

        if file_is_new:
            writer.writerow(HEADER)
            f.flush()

        # Background thread: keeps reading from the Arduino at all times
        listener = threading.Thread(target=listen_loop, args=(ser, writer, f), daemon=True)
        listener.start()

        # Main thread: whatever you type here gets sent to the Arduino
        try:
            while True:
                user_input = input()
                ser.write((user_input + "\n").encode())
        except KeyboardInterrupt:
            print("\nStopped.")


if __name__ == "__main__":
    main()
