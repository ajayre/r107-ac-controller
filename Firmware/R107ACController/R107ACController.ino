// R107/C107 AC Controller (manual climate control)
// (C) Andy Ayre, 2020, all rights reserved
// https://github.com/ajayre/r107-ac-controller
// Outputs to serial port at 57,600 baud

#include "src/Adafruit-MAX31855/Adafruit_MAX31855.h"
#include <avr/wdt.h>

// the version of this firmware
#define VERSION "1.00"

// define to 1 to simulate ac instead of reading thermocouple
#define SIMULATE_AC 0

// logic
#define FALSE (0)
#define TRUE (!FALSE)

// minimum evap temperature in celcius
// below this the freeze protection will start
#define MINIMUM_TEMPERATURE 2.0F
// the number of degrees in celcius the evap temp has to rise before
// exiting freeze protection
#define FREEZEPROTECTION_HYSTERESIS 1.0F
// the number of degrees in celcius the eval temp has to rise before
// turning compressor back on after reaching target
// larger value = worse performance of system
// smaller value = more cycling of compressor on/off when at target
#define TEMP_HYSTERESIS 1.0F
// size of temperature range controllable by user, in degrees celcius
#define TEMP_RANGE 15.0F

// pin to control the compressor
#define COMPRESSOR      6    // D6
// pin for on/off switch
#define CONTROL_SWITCH  7    // D7
// thermocouple chip select (uses hardware SPI)
#define CS_THERMOCOUPLE 8    // D8
// pin to control the blower motor
#define BLOWER_MOTOR    9    // D9
// pin for temperature control (analog)
#define TEMP_SETTING    A0
// pin for status LED
#define STATUS_LED       A3

// thermocouple errors
#define THERMOCOUPLE_ERROR_SCV 0x04 // short circuit to vcc
#define THERMOCOUPLE_ERROR_SCG 0x02 // short circuit to ground
#define THERMOCOUPLE_ERROR_OC  0x01 // open connection

// status LED control
#define STATUS_LED_ON  digitalWrite(STATUS_LED, LOW)
#define STATUS_LED_OFF digitalWrite(STATUS_LED, HIGH);
#define IS_STATUS_LED_ON (digitalRead(STATUS_LED) == 1 ? FALSE : TRUE)

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

// minimum on or off time in milliseconds
#define LED_PERIOD 200
// time between patterns as a multiple of LED_PERIOD
#define LED_PATTERN_OFF_TIME 4

// pattern to show on LED in the various state machine states
#define LED_PATTERN_READY            2
#define LED_PATTERN_RUNNING          3
#define LED_PATTERN_FREEZEPROTECTION 4

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

// modes of operation for the LED
typedef enum _ledmodes
{
  LED_Mode_Off,
  LED_Mode_On,
  LED_Mode_Error,
  LED_Mode_Pattern
} LEDMODES;

// the states used to flash patterns on the LED
typedef enum _ledpatternstates
{
  LED_Pattern_Flashing,
  LED_Pattern_Paused
} LEDPATTERNSTATES;

// access to thermocouple
static Adafruit_MAX31855 Thermocouple(CS_THERMOCOUPLE);
// current ac state
static ACSTATE ACState = OFF;

// LED handling
static LEDMODES LEDMode;
static unsigned long LEDNextTime;
static int LEDPatternCounter;
static LEDPATTERNSTATES LEDPatternState;
static int LEDPatternNumber;

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

  // set starting temp for simulation
  if (SimulatedTemp < 0) SimulatedTemp = AmbientTemperature;

  // if compressor is on then decrease temp 6 deg C per min
  if (IS_COMPRESSOR_ON)
  {
    SimulatedTemp -= 0.1;
    if (SimulatedTemp < 0) SimulatedTemp = 0;
  }
  // if compressor is off then increase temp 6 deg C per min
  else
  {
    SimulatedTemp += 0.1;
    if (SimulatedTemp > AmbientTemperature) SimulatedTemp = AmbientTemperature;
  }

  return SimulatedTemp;
}
#endif // SIMULATE_AC == 1

// Starts flashing a pattern on the LED
static void EnableLEDPattern
  (
  int PatternNumber                  // number of flashes before pausing
  )
{
  LEDMode = LED_Mode_Pattern;
  LEDNextTime = GetTime() + LED_PERIOD;
  LEDPatternCounter = 0;
  LEDPatternState = LED_Pattern_Flashing;
  LEDPatternNumber = PatternNumber;
  STATUS_LED_OFF;
}

// Gets the current time in milliseconds since last power on
static unsigned long GetTime
  (
  void
  )
{
  return millis();
}

// Checks if a timestamp is in the past, handles 32-bit timer overflow
static uint8_t IsTimeExpired
  (
  unsigned long timestamp            // timestamp to check
  )
{
  unsigned long time_now;

  time_now = millis();
  if (time_now >= timestamp)
  {
    if ((time_now - timestamp) < 0x80000000)
      return 1;
    else
      return 0;
  }
  else
  {
    if ((timestamp - time_now) >= 0x80000000)
      return 1;
    else
      return 0;
  }
}

// Call frequently to update the LED
static void LEDHandler
  (
  void
  )
{
  if (IsTimeExpired(LEDNextTime))
  {
    switch (LEDMode)
    {
      // turn LED off
    case LED_Mode_Off:
      STATUS_LED_OFF;
      break;

      // turn LED on
    case LED_Mode_On:
      STATUS_LED_ON;
      break;

      // show the error state on the LED
    case LED_Mode_Error:
      if (IS_STATUS_LED_ON)
      {
        STATUS_LED_OFF;
      }
      else
      {
        STATUS_LED_ON;
      }
      break;

      // show a specific pattern on the LED
    case LED_Mode_Pattern:
      switch (LEDPatternState)
      {
      case LED_Pattern_Flashing:
        if (IS_STATUS_LED_ON)
        {
          STATUS_LED_OFF;
        }
        else
        {
          STATUS_LED_ON;
          if (++LEDPatternCounter == LEDPatternNumber)
          {
            LEDPatternState = LED_Pattern_Paused;
            LEDPatternCounter = 0;
          }
        }
        break;

      case LED_Pattern_Paused:
        STATUS_LED_OFF;
        if (++LEDPatternCounter == LED_PATTERN_OFF_TIME)
        {
          LEDPatternState = LED_Pattern_Flashing;
          LEDPatternCounter = 0;
        }
        break;
      }
      break;
    }

    LEDNextTime = GetTime() + LED_PERIOD;
  }
}

// calculates the target evap temperature based on the current setting
// and measurements
// note - if TempSetting is greater than INSIDE_AIR_THRESHOLD then the
// switch is in the 'inside' range
// returns the target evap temperature in degrees C
// note - return value must be at least MINIMUM_TEMPERATURE
static double GetTargetEvapTemperature
(
  double TempSetting,                           // user's temp setting 0 (min) -> 100 (max)
  double AmbientTemperature,                    // in degrees C, taken from behind dashboard, NOT equal to outside temp
  double EvapTemperature                        // in degrees C
  )
{
  Serial.print("Temp setting = ");
  Serial.print(TempSetting);
  if (TempSetting > INSIDE_AIR_THRESHOLD)
    Serial.println(" (inside)");
  else
    Serial.println(" (fresh air)");

  // get controllable temperarure range
  double MinTemp = MINIMUM_TEMPERATURE;
  double MaxTemp = MinTemp + TEMP_RANGE;

  // scale the user's temperature setting over the min -> max range
  double TargetTemperature = MaxTemp - ((MaxTemp - MinTemp) * (TempSetting / 100.0));

  return TargetTemperature;
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

  // configure status LED and turn on
  pinMode(STATUS_LED, OUTPUT);
  // turn LED on
  LEDMode = LED_Mode_On;

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
    LEDMode = LED_Mode_Error;

    // wait for reset
    while (1);
  }

  wdt_reset();

  Serial.println("Ready");
  ACState = READY;
  EnableLEDPattern(LED_PATTERN_READY);
}

// the loop function runs over and over again until power down or reset
void loop
  (
  void
  )
{
  // feed watchdog
  wdt_reset();

  // update LED state
  LEDHandler();

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
    LEDMode = LED_Mode_Error;

    // wait for reset
    while (1);
  }

  // execute state machine
  switch (ACState)
  {
    case FREEZEPROTECTION:
      // if switch turned off then stop running
      if (!GET_CONTROL_SWITCH)
      {
        COMPRESSOR_OFF;
        BLOWER_OFF;
        Serial.println("Ready");
        ACState = READY;
        EnableLEDPattern(LED_PATTERN_READY);
        break;
      }

      if (EvapTemperature >= (MINIMUM_TEMPERATURE + FREEZEPROTECTION_HYSTERESIS))
      {
        Serial.println("Ready");
        ACState = READY;
        EnableLEDPattern(LED_PATTERN_READY);
      }
      break;

    // do nothing in these states
    case OFF:
    case INIT:
      break;

    default:
      Serial.print("Unknown state ");
      Serial.println(ACState);
      // wait for reset
      while (1);
      break;

    case READY:
      // if switch turned on then ac now running
      if (GET_CONTROL_SWITCH)
      {
        ACState = RUNNING;
        Serial.println("Running");
        BLOWER_ON;
        EnableLEDPattern(LED_PATTERN_RUNNING);
      }
      else
      {
        BLOWER_OFF;
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
        EnableLEDPattern(LED_PATTERN_READY);
        break;
      }

      // check for minimum evap temo
      if (EvapTemperature <= MINIMUM_TEMPERATURE)
      {
        COMPRESSOR_OFF;
        Serial.println("Freeze protection");
        ACState = FREEZEPROTECTION;
        EnableLEDPattern(LED_PATTERN_FREEZEPROTECTION);
        break;
      }

      // get user's temp setting and scale to 0 -> 100
      double TempSetting = RAW_TEMP_SETTING;
      TempSetting = TempSetting / 1023 * 100;
      if (TempSetting < 0) TempSetting = 0;
      if (TempSetting > 100) TempSetting = 100;

      // get the target temperature in degrees C
      double TargetEvapTemperature = GetTargetEvapTemperature(TempSetting, AmbientTemperature, EvapTemperature);
      if (TargetEvapTemperature < MINIMUM_TEMPERATURE) TargetEvapTemperature = MINIMUM_TEMPERATURE;
      Serial.print("Target temp = ");
      Serial.println(TargetEvapTemperature);

      // turn compressor on or off as needed
      if (EvapTemperature <= TargetEvapTemperature)
      {
        COMPRESSOR_OFF;
      }
      else if (EvapTemperature > (TargetEvapTemperature + TEMP_HYSTERESIS))
      {
        COMPRESSOR_ON;
      }
      break;
  }

  wdt_reset();
}
