# SonoffBoilerplate

This is a fork of [tzapu](https://github.com/tzapu) SonoffBoilerplate project, currently deleted from GitHub.

This is a replacement firmware (Arduino IDE with ESP8266 core needed) for the ESP8266 based Sonoff devices. Use it as a starting block for customizing your Sonoff.

## What's a "Sonoff"?
Sonoff is just a small ESP8266 based module, that can toggle mains power on it's output. It has everything included in a nice plastic package.
See more here [Sonoff manufacturer website](https://www.itead.cc/sonoff-wifi-wireless-switch.html)

## Features include
- wifi credentials configuration/onboarding using [WiFiManager](https://github.com/tzapu/WiFiManager)
- web configuration portal to setup module type, sensors, Blynk, MQTT, etc
- Blynk integration
- MQTT integration
- HTTP sever API
- OTA over the air firmware update
- turn off and on relay from onboard button

If you want a more complete/complex firmware you should check out the [Sonoff-Tasmota](https://github.com/arendst/Sonoff-Tasmota) project.

## Getting started
First of all you will need to solder a 4 or 5 pin header on your Sonoff so you can flash the new firmware.

You will need to download any libraries included, they should all have URLs in the source code mentioned, or you can find them in the Arduino Library Manager.

After you have the header, the libraries installed and a serial to usb dongle ready, power up the module while pressing the onboard button. This should put it into programming mode.

Flash the firmware, the module should reset afterwards and the green LED should be blinking.
Slow blink = connecting
Fast blink = configuration portal started

Being your first run, connect to the Access Point the module created and configure it. If you don t get a configuration popup when connecting, open 192.168.4.1 in your browser.

After it's configured and connected, the green LED should stay lit, and the relay should be enabled (this is the default).

### Over-The-Air updates
OTA should also be enabled now and you can do future updates over the air. 
It uses the basic ArduinoOTA available in Arduino IDE port of the ESP8266 core.

### Blynk App 
Blynk App is supported in both local server or cloud mode. 
Add the token and the server details in the web config portal.
Relay pins are V1-V4 (depend on module type), sensor pins are V11-V14. V30 is restart pin and V31 is full reset pin.

### MQTT Support
MQTT has now been added
You can send messages to `cmnd/%topic%/POWER%relay_number%` with the following parameters:
- no parameter (blank) and the device will send it's status back
- 'on' to turn relay on
- 'off' to turn relay off
- 'toggle' to toggle relay
The status will come as a response on topic `stat/%topic%/POWER%relay_number%`
The sensor states will published on `stat/%topic%/SENSOR%sensor_number%`

### More information

Part 1 - Original Sonoff Introduction

[Sonoff (ESP8266) reprogramming – Control Mains from Anywhere](https://tzapu.com/sonoff-firmware-boilerplate-tutorial/) 


Part 2 - Original Sonoff firmware replacement

[Sonoff (ESP8266) Part 2 – Control with Blynk App on iOS or Android](https://tzapu.com/sonoff-esp8266-control-blynk-ios-android/)

Part 3 - Sonoff POW and S20 Smart Socket Introduction

[Sonoff POW and S20 Smart Socket from ITEAD](https://tzapu.com/sonoff-pow-and-s20-smart-socket-itead/)

