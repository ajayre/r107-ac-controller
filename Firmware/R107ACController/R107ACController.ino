// R107/C107 AC Controller (manual climate control)
// (C) Andy Ayre, 2020, all rights reserved
// https://github.com/ajayre/r107-ac-controller

#include "src/Adafruit-MAX31855/Adafruit_MAX31855.h"
#include <avr/wdt.h>

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

// pin to control the blower motor
#define BLOWER_MOTOR 9 // D9

// pin to control the compressor
#define COMPRESSOR 10 // D10

// thermocouple errors
#define THERMOCOUPLE_ERROR_SCV 0x04 // short circuit to vcc
#define THERMOCOUPLE_ERROR_SCG 0x02 // short circuit to ground
#define THERMOCOUPLE_ERROR_OC  0x01 // open connection

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

// blower control macros (connect to NO on relay)
#if _DEBUG == 1
#define BLOWER_OFF digitalWrite(BLOWER_MOTOR, LOW); Serial.println("Blower off");
#define BLOWER_ON  digitalWrite(BLOWER_MOTOR, HIGH); Serial.println("Blower on");
#else
#define BLOWER_OFF digitalWrite(BLOWER_MOTOR, LOW)
#define BLOWER_ON  digitalWrite(BLOWER_MOTOR, HIGH)
#endif

// compressor control macros (connect to NO on relay)
#if _DEBUG == 1
#define COMPRESSOR_OFF digitalWrite(COMPRESSOR, LOW); Serial.println("Compressor off")
#define COMPRESSOR_ON  digitalWrite(COMPRESSOR, HIGH); Serial.println("Compressor on")
#else
#define COMPRESSOR_OFF digitalWrite(COMPRESSOR, LOW)
#define COMPRESSOR_ON  digitalWrite(COMPRESSOR, HIGH)
#endif // _DEBUG == 1

// the setup function runs once when you press reset or power the board
void setup
  (
  void
  )
{
  // fixme - to do - enable brownout detection

  // enable watchdog timers with eight second timeout
  // fixme - to do - this does not work with the 'old' bootloader, it will continually reset after the first
  // watchdog reset. need to upgrade to the new bootloader
  //wdt_enable(WDTO_8S);

  // configure blower motor
  pinMode(BLOWER_MOTOR, OUTPUT);
  BLOWER_OFF;

  // configure compressor
  pinMode(COMPRESSOR, OUTPUT);
  COMPRESSOR_OFF;

  // start serial output
  Serial.begin(57600);
  while (!Serial) delay(1);
  Serial.flush();

  wdt_reset();

  // print banner
  Serial.println("R107 C107 AC Controller (C) Andy Ayre 2020");
  Serial.println("andy@britishideas.com");
  Serial.print("Version ");
  Serial.println(VERSION);
  Serial.println();

  Serial.println("Init");
  ACState = INIT;

  // wait for thermocouple to become ready, comes from the adafruit example
  delay(500);

  wdt_reset();

  // start thermocouple
  if (!Thermocouple.begin())
  {
    Serial.println("Failed to initalize thermocouple");
    ACState = OFF;

    // wait for reset
    while (1);
  }

  wdt_reset();

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
  double AmbientTemperature = Thermocouple.readInternal();

  // get current evap temperature and check for fault
  bool Faulted = false;
  double EvapTemperature = Thermocouple.readCelsius();
  if (isnan(EvapTemperature))
  {
    Serial.print("Thermocouple fault ");
    uint8_t ThermocoupleError = Thermocouple.readError();
    if ((ThermocoupleError & THERMOCOUPLE_ERROR_SCV) == THERMOCOUPLE_ERROR_SCV) Serial.print("(short-circuit to Vcc)");
    if ((ThermocoupleError & THERMOCOUPLE_ERROR_SCG) == THERMOCOUPLE_ERROR_SCG) Serial.print("(short-circuit to Gnd)");
    if ((ThermocoupleError & THERMOCOUPLE_ERROR_OC)  == THERMOCOUPLE_ERROR_OC)  Serial.print("(open circuit)");
    Serial.println();

    ACState = FAULT;
    Faulted = true;
    COMPRESSOR_OFF;
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

  // check if SPI is non-functional
  if ((AmbientTemperature == 0) && (EvapTemperature == 0) && !Faulted)
  {
    Serial.println("No response from thermocouple controller");
    ACState = OFF;
    COMPRESSOR_OFF;

    // wait for reset
    while (1);
  }

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

        // wait for reset
        while (1);
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
        COMPRESSOR_OFF;
      }
      // cycle compressor as needed, go back to ready if switch turned off
      else
      {
        // calculate the current level of cooling (difference between ambient temp and evap temp)
        double Cooling = 0;
        if (AmbientTemperature > EvapTemperature) Cooling = AmbientTemperature - EvapTemperature;

#if _DEBUG == 1
        Serial.print("Cooling = ");
        Serial.println(Cooling);
#endif // _DEBUG == 1

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

  wdt_reset();
}
