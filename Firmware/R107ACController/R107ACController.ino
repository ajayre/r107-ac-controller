// R107/C107 AC Controller (manual climate control)
// (C) Andy Ayre, 2020, all rights reserved
// https://github.com/ajayre/r107-ac-controller
// Outputs to serial port at 57,600 baud

#include "src/Adafruit-MAX31855/Adafruit_MAX31855.h"
#include <avr/wdt.h>

// the version of this firmware
#define VERSION "1.00"

// define to 1 to simulate ac instead of reading thermocouple
#define SIMULATE_AC 1

// minimum evap temperature in celcius
// below this the freeze protection will start
#define MINIMUM_TEMPERATURE 4.0F
// the number of degrees in celcius the evap temp has to rise before
// exiting freeze protection
#define FREEZEPROTECTION_HYSTERESIS 1.0F

// the number of degrees in celcius the eval temp has to rise before
// turning compressor back on after reaching target
#define TEMP_HYSTERESIS 1.0F

// thermocouple chip select (uses hardware SPI)
#define CS_THERMOCOUPLE 8 // D8
// pin to control the blower motor
#define BLOWER_MOTOR 9    // D9
// pin to control the compressor
#define COMPRESSOR 10     // D10
// pin for on/off switch
#define CONTROL_SWITCH 7  // D7
// pin for temperature control (analog)
#define TEMP_SETTING A0

// thermocouple errors
#define THERMOCOUPLE_ERROR_SCV 0x04 // short circuit to vcc
#define THERMOCOUPLE_ERROR_SCG 0x02 // short circuit to ground
#define THERMOCOUPLE_ERROR_OC  0x01 // open connection

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
#define IS_COMPRESSOR_ON digitalRead(COMPRESSOR)

// control switch macro
// returns 0 for off, 1 for on
#define GET_CONTROL_SWITCH (!(digitalRead(CONTROL_SWITCH) & 0x01))

// temp setting macro
// returns 0 = min, 1023 = max
#define RAW_TEMP_SETTING analogRead(TEMP_SETTING)
// raw temp setting where switch moves from fresh air to inside air
#define RAW_INSIDE_AIR_THRESHOLD 800
// scaled temp setting threshold
#define INSIDE_AIR_THRESHOLD (RAW_INSIDE_AIR_THRESHOLD / 1023.0F * 100.0F)

// never use simulation for release builds
#if _DEBUG == 0
#undef SIMULATE_AC
#define SIMULATE_AC 0
#endif // _DEBUG == 0

// ac state machine states
typedef enum _acstates
{
  OFF,
  INIT,
  READY,
  RUNNING,
  FREEZEPROTECTION
} ACSTATE;

// access to thermocouple
static Adafruit_MAX31855 Thermocouple(CS_THERMOCOUPLE);
// current ac state
static ACSTATE ACState = OFF;

#if SIMULATE_AC == 1
static unsigned long SimulationTimestamp = 0;
static double SimulatedTemp = -1;
static double GetSimulatedTemperature
  (
  double AmbientTemperature
  )
{
  // don't do anything until one second has passed
  if (millis() - SimulationTimestamp < 1000)
  {
    return;
  }

  SimulationTimestamp = millis();

  if (SimulatedTemp < 0) SimulatedTemp = AmbientTemperature;

  // if compressor is on then decrease temp
  if (IS_COMPRESSOR_ON)
  {
    SimulatedTemp -= 0.3;
    if (SimulatedTemp < 0) SimulatedTemp = 0;
  }
  // if compressor is off then increase temp
  else
  {
    SimulatedTemp += 0.3;
    if (SimulatedTemp > AmbientTemperature) SimulatedTemp = AmbientTemperature;
  }

  return SimulatedTemp;
}
#endif // SIMULATE_AC == 1

// calculates the target evap temperature based on the current setting
// and measurements
// note - if TempSetting is greater than INSIDE_AIR_THRESHOLD then the
// switch is in the 'inside' range
// returns the target evap temperature in degrees C
// note - return value must be at least MINIMUM_TEMPERATURE
static double GetTargetEvapTemperature
(
  double TempSetting,                           // user's temp setting 0 (min) -> 100 (max)
  double AmbientTemperature,                    // in degrees C
  double EvapTemperature                        // in degrees C
  )
{
  Serial.print("Temp setting = ");
  Serial.print(TempSetting);
  if (TempSetting > INSIDE_AIR_THRESHOLD)
    Serial.println(" (inside)");
  else
    Serial.println(" (fresh air)");

  // fixme - to do
  return MINIMUM_TEMPERATURE + 1;
}

// the setup function runs once when you press reset or power the board
void setup
  (
  void
  )
{
  // enable watchdog timers with eight second timeout
  // fixme - to do - this does not work with the 'old' bootloader, it will continually reset after the first
  // watchdog reset. need to upgrade to the new bootloader
  //wdt_enable(WDTO_8S);

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

  // configure blower motor
  pinMode(BLOWER_MOTOR, OUTPUT);
  BLOWER_OFF;

  // configure compressor
  pinMode(COMPRESSOR, OUTPUT);
  COMPRESSOR_OFF;

  // configure control switch
  pinMode(CONTROL_SWITCH, INPUT_PULLUP);
  
  wdt_reset();

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
  double EvapTemperature;
#if SIMULATE_AC == 1
  EvapTemperature = GetSimulatedTemperature(AmbientTemperature);
#else
  EvapTemperature = Thermocouple.readCelsius();
#endif // SIMULATE_AC
  if (isnan(EvapTemperature))
  {
    Serial.print("Thermocouple fault ");
    uint8_t ThermocoupleError = Thermocouple.readError();
    if ((ThermocoupleError & THERMOCOUPLE_ERROR_SCV) == THERMOCOUPLE_ERROR_SCV) Serial.print("(short-circuit to Vcc)");
    if ((ThermocoupleError & THERMOCOUPLE_ERROR_SCG) == THERMOCOUPLE_ERROR_SCG) Serial.print("(short-circuit to Gnd)");
    if ((ThermocoupleError & THERMOCOUPLE_ERROR_OC)  == THERMOCOUPLE_ERROR_OC)  Serial.print("(open circuit)");
    Serial.println();

    COMPRESSOR_OFF;

    // wait for reset
    while (1);
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
  if ((AmbientTemperature == 0) && (EvapTemperature == 0))
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

    case READY:
      // if switch turned on then ac now running
      if (GET_CONTROL_SWITCH)
      {
        ACState = RUNNING;
        Serial.println("Running");
        BLOWER_ON;
      }
      break;

    case RUNNING:
      // if switch turned off then stop running
      if (!GET_CONTROL_SWITCH)
      {
        COMPRESSOR_OFF;
        BLOWER_OFF;
        Serial.println("Ready");
        ACState = READY;
        break;
      }

      if (EvapTemperature <= MINIMUM_TEMPERATURE)
      {
        COMPRESSOR_OFF;
        Serial.println("Freeze protection");
        ACState = FREEZEPROTECTION;
        break;
      }

      // calculate the current level of cooling (difference between ambient temp and evap temp)
      double Cooling = 0;
      if (AmbientTemperature > EvapTemperature) Cooling = AmbientTemperature - EvapTemperature;

      // get user's temp setting and scale to 0 -> 100
      double TempSetting = RAW_TEMP_SETTING;
      TempSetting = TempSetting / 1023 * 100;
      if (TempSetting < 0) TempSetting = 0;
      if (TempSetting > 100) TempSetting = 100;

      // get the target temperature in degrees C
      double TargetEvapTemperature = GetTargetEvapTemperature(TempSetting, AmbientTemperature, EvapTemperature);
      Serial.print("Target evap temp = ");
      Serial.println(TargetEvapTemperature);

#if _DEBUG == 1
      Serial.print("Cooling = ");
      Serial.println(Cooling);
#endif // _DEBUG == 1

      if (EvapTemperature <= TargetEvapTemperature)
      {
        COMPRESSOR_OFF;
      }
      else if (EvapTemperature > (TargetEvapTemperature + TEMP_HYSTERESIS))
      {
        COMPRESSOR_ON;
      }
      break;

    // wait for evap temp to rise again
    case FREEZEPROTECTION:
      // if switch turned off then stop running
      if (!GET_CONTROL_SWITCH)
      {
        COMPRESSOR_OFF;
        BLOWER_OFF;
        Serial.println("Ready");
        ACState = READY;
        break;
      }

      if (EvapTemperature >= (MINIMUM_TEMPERATURE + FREEZEPROTECTION_HYSTERESIS))
      {
        COMPRESSOR_OFF;
        BLOWER_OFF;
        Serial.println("Ready");
        ACState = READY;
      }
      break;
  }

  wdt_reset();
}
