// R107/C107 AC Controller (manual climate control)
// (C) Andy Ayre, 2020, all rights reserved

#include "src/Adafruit-MAX31855/Adafruit_MAX31855.h"

// the version of this firmware
#define VERSION "1.00"

// minimum evap temperature in celcius
// below this the freeze protection will start
#define MINIMUM_TEMPERATURE 3.0F

// the number of degrees in celcius the evap temp has to rise before
// exiting freeze protection
#define FREEZEPROTECTION_HYSTERESIS 1.0F

// thermocouple chip select (uses hardware SPI)
#define CS_THERMOCOUPLE 8 // D8

/*
 Notes:
 Don't let compressor cycle on and off too often - either use a minimum time or hysteresis
 SS pin must be pulled high
*/

// ac state machine states
typedef enum _acstates
{
  OFF,
  INIT,
  READY,
  RUNNING,
  FREEZEPROTECTION,
  FAULT
} ACSTATE;

// access to thermocouple
static Adafruit_MAX31855 Thermocouple(CS_THERMOCOUPLE);
// current ac state
static ACSTATE ACState = OFF;

// turns the compressor off
static void CompressorOff
  (
  void
  )
{
  // fixme - to do
}

// turns the compressor on
static void CompressorOn
  (
  void
  )
{
  // fixme - to do
}

// the setup function runs once when you press reset or power the board
void setup
  (
  void
  )
{
  // fixme - to do - enable watchdog, enable brownout detection

  // start serial output
  Serial.begin(57600);
  while (!Serial) delay(1);
  Serial.flush();

  // print banner
  Serial.println("R107 C107 AC Controller (C) Andy Ayre 2020");
  Serial.println("andy@britishideas.com");
  Serial.print("Version ");
  Serial.println(VERSION);
  Serial.println();

  Serial.println("Init");
  ACState = INIT;

  // wait for thermocouple to become ready
  delay(500);

  // start thermocouple
  if (!Thermocouple.begin())
  {
    Serial.println("Failed to initalize thermocouple");
    ACState = OFF;
    while (1); // fixme - reset device
  }

  Serial.println("Ready");
  ACState = READY;
}

// the loop function runs over and over again until power down or reset
void loop
  (
  void
  )
{
  // get temperature of cabin
  //Serial.print("Internal Temp = ");
  double AmbientTemperature = Thermocouple.readInternal();

  // get current evap temperature and check for fault
  bool Faulted = false;
  double EvapTemperature = Thermocouple.readCelsius();
  if (isnan(EvapTemperature))
  {
    Serial.println("Fault");
    ACState = FAULT;
    Faulted = true;
    CompressorOff();
    // fixme - to do - record fault time
  }
#if _DEBUG == 1
  else
  {
    Serial.print("Evap = ");
    Serial.println(EvapTemperature);
    delay(250);
  }
#endif // _DEBUG == 1

  // execute state machine
  switch (ACState)
  {
    // do nothing in these states
    default:
    case OFF:
    case INIT:
      break;

    case FAULT:
      // if fault has cleared then go back to being ready
      if (!Faulted)
      {
        Serial.println("Ready");
        ACState = READY;
      }
      // if too much time spent faulted then reset device
      else
      {
        // fixme - to do
      }
      break;

    case READY:
      // fixme - if switch turned on then start running
      // fixme - remove
      Serial.println("Running");
      ACState = RUNNING;
      break;

    case RUNNING:
      if (EvapTemperature <= MINIMUM_TEMPERATURE)
      {
        Serial.println("Freeze protection");
        ACState = FREEZEPROTECTION;
        CompressorOff();
      }
      // cycle compressor as needed, go back to ready if switch turned off
      else
      {
        // calculate the current level of cooling (difference between ambient temp and evap temp)
        double Cooling = 0;
        if (AmbientTemperature > EvapTemperature) Cooling = AmbientTemperature - EvapTemperature;

        // fixme - to do
      }
      break;

    // wait for evap temp to rise again
    case FREEZEPROTECTION:
      if (EvapTemperature >= (MINIMUM_TEMPERATURE + FREEZEPROTECTION_HYSTERESIS))
      {
        Serial.println("Ready");
        ACState = READY;
      }
      break;
  }
}
