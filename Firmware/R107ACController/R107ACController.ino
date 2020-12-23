// R107/C107 AC Controller (manual climate control)
// (C) Andy Ayre, 2020, all rights reserved

#include "src/Adafruit-MAX31855/Adafruit_MAX31855.h"

// thermocouple chip select
#define MAXCS   8  // D8

// access to thermocouple
static Adafruit_MAX31855 Thermocouple(MAXCS);

// the setup function runs once when you press reset or power the board
void setup() {
  // start serial output
  Serial.begin(57600);
  while (!Serial) delay(1);
  Serial.flush();

  // print banner
  Serial.print("R107 C107 AC Controller (C) Andy Ayre 2020\n");

  // wait for thermocouple to become ready
  delay(500);

  // start thermocouple
  if (!Thermocouple.begin())
  {
    Serial.print("Failed to initalize thermocouple\n");
    while (1); // fixme
  }
}

// the loop function runs over and over again until power down or reset
void loop() {
  Serial.print("Internal Temp = ");
  Serial.println(Thermocouple.readInternal());

  double c = Thermocouple.readCelsius();
  if (isnan(c)) {
    Serial.println("Something wrong with thermocouple!");
  }
  else {
    Serial.print("C = ");
    Serial.println(c);
  }

  delay(1000);
}
