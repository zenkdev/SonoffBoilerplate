
# SonoffBoilerplate

This is a fork of [tzapu](https://github.com/tzapu) SonoffBoilerplate project, currently deleted from GitHub.

This is a replacement firmware (Arduino IDE with ESP8266 core needed) for the ESP8266 based Sonoff devices. Use it as a starting block for customizing your Sonoff.

## What's a "Sonoff"?
Sonoff is just a small ESP8266 based module, that can toggle mains power on it's output. It has everything included in a nice plastic package.
See more here [Sonoff manufacturer website](https://www.itead.cc/sonoff-wifi-wireless-switch.html)

## Supported modules
- Sonoff Basic
- WeMos D1 mini

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

In the Arduino IDE select from `Tools Board Generic ESP8266 Module` ( `Tools Board Generic ESP8285 Module` for CH4 version) and set the following options:
- Upload Using: Serial
- Flash Mode: DOUT
- Flash Frequency: 40MHz
- CPU Frequency: 80MHz
- Flash Size: 1M (64K SPIFFS)
- Debug Port: Disabled
- Debug Level: None
- Reset Method: ck
- Upload Speed: 115200
- Port: Your COM port connected to sonoff

Flash the firmware, the module should reset afterwards and the green LED should be blinking.

Being your first run, connect to the Access Point the module created and configure it. If you don t get a configuration popup when connecting, open 192.168.4.1 in your browser.

After it's configured and connected, the green LED should stay off (this is the default).

### Over-The-Air updates
OTA should also be enabled now and you can do future updates over the air. 
It uses the basic ArduinoOTA available in Arduino IDE port of the ESP8266 core.

### Blynk App 
Blynk App is supported in both local server or cloud mode. 
Add the token and the server details in the web config portal.
- V1-V4 are the relay pins (depend on module type).
- V11-V14 are the sensor pins.
- V30 is restart pin.
- V31 is reset pin.

### MQTT Support
MQTT has now been added
You can send messages to `cmnd/%topic%/POWER%relay_number%` with the following parameters:
- no parameter (blank) and the device will send it's status back
- 'on' to turn relay on
- 'off' to turn relay off
- 'toggle' to toggle relay

The device uses the following topics for publication:
- The relay status will come on topic `stat/%topic%/POWER%relay_number%`.
- The sensor states will published on `stat/%topic%/SENSOR%sensor_number%`.
- Device telemetry will published on `tele/%topic%/STATE`.

### Button functions
- 1 short press: Toggles the relay.
- 2 short presses: Toggles the relay. For Sonoff Dual this will switch relay 2.
- 3 short presses: Start Wifi smartconfig allowing for SSID and Password configuration using an Android mobile phone with the [ESP8266 SmartConfig](https://play.google.com/store/apps/details?id=com.cmmakerclub.iot.esptouch) app.
- 4 short presses: Start Wifi manager providing an Access Point with IP address 192.168.4.1 and a web server allowing the configuration of Wifi parameters.
- 5 short presses: Start Wifi Protected Setup (WPS) allowing for SSID and Password configuration using the routers WPS button or webpage.
- 6 short presses: Will restart the module.
- Pressing the button for over four seconds: Reset settings to defaults as defined in config.h and restarts the device.

### More information

Part 1 - Original Sonoff Introduction

[Sonoff (ESP8266) reprogramming – Control Mains from Anywhere](https://tzapu.com/sonoff-firmware-boilerplate-tutorial/) 


Part 2 - Original Sonoff firmware replacement

[Sonoff (ESP8266) Part 2 – Control with Blynk App on iOS or Android](https://tzapu.com/sonoff-esp8266-control-blynk-ios-android/)

Part 3 - Sonoff POW and S20 Smart Socket Introduction

[Sonoff POW and S20 Smart Socket from ITEAD](https://tzapu.com/sonoff-pow-and-s20-smart-socket-itead/)

