#ifdef INCLUDE_BLYNK_SUPPORT
/**********
   VPIN % 5
   0 off
   1 on
   2 toggle
   3 value
   4 led
 ***********/

#include <BlynkSimpleEsp8266.h>

#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space

static bool BLYNK_ENABLED = true;

void Blynk_setup() {
  if (strlen(settings.blynkToken) == 0) {
    BLYNK_ENABLED = false;
  }
  if (BLYNK_ENABLED) {
    Blynk.config(settings.blynkToken, settings.blynkServer, atoi(settings.blynkPort));
  }
}

void Blynk_loop() {
  //blynk connect and run loop
  if (BLYNK_ENABLED) {
    Blynk.run();
  }
}

void Blynk_update(int channel) {
  int state = digitalRead(SONOFF_RELAY_PINS[channel]);
  Blynk.virtualWrite(channel * 5 + 4, state * 255);
}

BLYNK_WRITE_DEFAULT() {
  int pin = request.pin;
  int channel = pin / 5;
  int action = pin % 5;
  int a = param.asInt();
  if (a != 0) {
    switch (action) {
      case 0:
        turnOff(channel);
        break;
      case 1:
        turnOn(channel);
        break;
      case 2:
        toggle(channel);
        break;
      default:
        Serial.print("unknown action");
        Serial.print(action);
        Serial.print(channel);
        break;
    }
  }
}

BLYNK_READ_DEFAULT() {
  // Generate random response
  int pin = request.pin;
  int channel = pin / 5;
  int action = pin % 5;
  Blynk.virtualWrite(pin, digitalRead(SONOFF_RELAY_PINS[channel]));

}

//restart - button
BLYNK_WRITE(30) {
  int a = param.asInt();
  if (a != 0) {
    restart();
  }
}

//reset - button
BLYNK_WRITE(31) {
  int a = param.asInt();
  if (a != 0) {
    reset();
  }
}

#endif

