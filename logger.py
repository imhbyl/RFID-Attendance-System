"""
PC-side attendance logger.

Receives from Arduino:

Name,AdmissionNo,RollNo,UID,Date,Time

Example:

Hubayl,20240057,12,567FFA03,17/09/2026,16:30:42

Then saves everything to:

attendance_log.csv
"""

import serial
import csv
import os
import time


# ==================================================
# SETTINGS
# ==================================================

PORT = "COM11"       # CHANGE THIS if your Arduino uses another port
BAUD = 9600

LOGFILE = "attendance_log.csv"


# ==================================================
# MAIN PROGRAM
# ==================================================

def main():

    print("===================================")
    print(" RFID ATTENDANCE LOGGER")
    print("===================================")
    print()

    print(f"Connecting to Arduino on {PORT}...")

    try:
        ser = serial.Serial(
            PORT,
            BAUD,
            timeout=1
        )

    except serial.SerialException as e:

        print()
        print("ERROR: Could not connect to Arduino.")
        print()
        print("Possible reasons:")
        print("1. Wrong COM port")
        print("2. Arduino Serial Monitor is open")
        print("3. Another program is using the COM port")
        print("4. Arduino is disconnected")
        print()
        print("Arduino error:")
        print(e)

        return


    # Arduino resets when serial connection opens.
    # Give it a couple seconds to restart.
    time.sleep(2)


    print()
    print(f"Connected successfully to {PORT}")
    print("Waiting for attendance scans...")
    print("Press Ctrl+C to stop.")
    print()


    # ==================================================
    # CREATE CSV FILE IF IT DOES NOT EXIST
    # ==================================================

    file_exists = os.path.isfile(LOGFILE)


    with open(
        LOGFILE,
        "a",
        newline="",
        encoding="utf-8"
    ) as f:

        writer = csv.writer(f)


        # Add header only when creating a new file
        if not file_exists or os.path.getsize(LOGFILE) == 0:

            writer.writerow([
                "Name",
                "Admission No",
                "Roll No",
                "UID",
                "Date",
                "Time"
            ])

            f.flush()


        # ==================================================
        # LISTEN FOR ARDUINO DATA
        # ==================================================

        while True:

            try:

                line = ser.readline().decode(
                    "utf-8",
                    errors="ignore"
                ).strip()


                # Ignore empty lines
                if not line:
                    continue


                print(f"Received: {line}")


                # ==================================================
                # IGNORE ARDUINO STARTUP / OTHER SERIAL MESSAGES
                # ==================================================

                if line.startswith("Name,AdmissionNo"):

                    continue


                # ==================================================
                # SPLIT CSV DATA
                # ==================================================

                parts = line.split(",")


                # We expect exactly 6 fields:
                #
                # Name
                # AdmissionNo
                # RollNo
                # UID
                # Date
                # Time

                if len(parts) != 6:

                    print(
                        "Ignored: unexpected data format"
                    )

                    continue


                name = parts[0]
                admission_no = parts[1]
                roll_no = parts[2]
                uid = parts[3]
                date = parts[4]
                clock_time = parts[5]


                # ==================================================
                # SAVE TO CSV
                # ==================================================

                writer.writerow([
                    name,
                    admission_no,
                    roll_no,
                    uid,
                    date,
                    clock_time
                ])

                f.flush()


                # ==================================================
                # DISPLAY RESULT
                # ==================================================

                print()
                print("===================================")
                print(" ATTENDANCE RECORDED")
                print("===================================")
                print(f"Name         : {name}")
                print(f"Admission No : {admission_no}")
                print(f"Roll No      : {roll_no}")
                print(f"RFID UID     : {uid}")
                print(f"Date         : {date}")
                print(f"Time         : {clock_time}")
                print("===================================")
                print()


            except KeyboardInterrupt:

                print()
                print("Logger stopped.")

                break


            except Exception as e:

                print("Error:", e)


    ser.close()


# ==================================================
# START PROGRAM
# ==================================================

if __name__ == "__main__":
    main()