/*
  Set RTC Time — run this once
  -----------------------------
  Sets the DS1307's clock to an exact hardcoded time. Run once, then
  switch back to the main attendance sketch — the RTC keeps time on
  its own afterward thanks to its backup battery.
*/

#include <Wire.h>
#include <RTClib.h>

RTC_DS1307 rtc;

void setup() {
  Serial.begin(9600);
  Wire.begin();

  if (!rtc.begin()) {
    Serial.println("RTC not found. Check wiring.");
    while (1);
  }

  // year, month, day, hour, minute, second
  rtc.adjust(DateTime(2026, 9, 19, 0, 58, 0));

  Serial.println("RTC time has been set!");

  DateTime now = rtc.now();
  Serial.print("Current RTC time: ");
  Serial.print(now.year());  Serial.print("/");
  Serial.print(now.month()); Serial.print("/");
  Serial.print(now.day());   Serial.print("  ");
  Serial.print(now.hour());  Serial.print(":");
  Serial.print(now.minute()); Serial.print(":");
  Serial.println(now.second());
}

void loop() {
  // nothing — this sketch only needs to run once
}
