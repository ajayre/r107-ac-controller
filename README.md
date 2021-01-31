# R107/C107 Air Conditioning Controller

This is an AC controller for the original Mercedes-Benz R107/C107 manual climate control system. The original system is identified by four vertical levers.

The problem with the AC switch is the delicate capillary tube that senses the evaporator temperature. With age these become brittle and snap off while working on items in the central console. Once broken they are not repairable. Replacements are not available to purchase.

The aim of this project is to replace the Ranco electro-mechanical part of the AC switch with a modern electronic equivalent.

This work is copyright (C) Andrew Ayre 2021 and no commercial use is permitted.

USE AT YOUR OWN RISK. NO WARRANTY, EXPRESS OR IMPLIED.

## Prototype Hardware

The prototype uses the following hardware:

* Arduino Nano - https://www.arduino.cc/en/pmwiki.php?n=Main/ArduinoBoardNano
* Adafruit Thermocouple Amplifier MAX31855 breakout board - https://www.adafruit.com/product/269
* Adafruit Power Relay FeatherWing - https://www.adafruit.com/product/3191

## Development Tools

* Arduino IDE - https://www.arduino.cc/en/software
* Install Microsoft Visual Studio Community Edition 2017 - https://visualstudio.microsoft.com/vs/older-downloads/
* Install Visual Micro - https://marketplace.visualstudio.com/items?itemName=VisualMicro.ArduinoIDEforVisualStudio

## Custom Board

### Programming the Bootloader

Use the Arduino IDE, choose the ATmega328P and your ISP programmer (e.g. USBtinyISP). If you choose to power the board from the programmer then there is no need to provide power via USP or 12V.

### Programming the Firmware

Set the target to 'ATmega328P (Arduino Nano)'.
Remove the RUN jumper. Replace the jumper after programming.

### LED Operation

The LED will turn on when firmware execution starts. Note that there is a delay of around two or three seconds while the bootloader runs and the LED will be off during this time.

* LED off = power supply problem
* LED on solid = in the 'ready' state, waiting for AC to be turned on. Compressor and blower are off
* LED continually flashing = error
* LED flashing twice = in the 'running' state, blower is on and compressor cycles as needed
* LED flashing three times = in the 'freeze protection' state, blower in on and compressor is off

### RUN Jumper

The RUN jumper should be installed when the board is installed in a vehicle. It needs to be removed during programming. The purpose of the RUN jumper is to stop noise on the receive line from keeping the board in the bootloader mode and stopping firmware from executing.
