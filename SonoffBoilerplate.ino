/********************************************
   1MB flash sizee

   sonoff header
   1 - vcc 3v3
   2 - rx
   3 - tx
   4 - gnd
   5 - gpio 14

   esp8266 connections
   gpio  0 - button
   gpio 12 - relay
   gpio 13 - green led - active low
   gpio 14 - pin 5 on header
********************************************/

#define   SONOFF_BUTTON             D0
#define   SONOFF_INPUT              D2 // Motion detector
#define   SONOFF_LED                LED_BUILTIN
#define   SONOFF_AVAILABLE_CHANNELS 1
const int SONOFF_RELAY_PINS[4] =    {D5, D6, D7, D8};

//if this is false, led is used to signal startup state, then always on
//if it s true, it is used to signal startup state, then mirrors relay state
//S20 Smart Socket works better with it false
#define SONOFF_LED_RELAY_STATE      true

#define HOSTNAME "sonoff-sensor"

#define PRESENCE_SENSOR
#define NOISE_LEVEL A0 // analog input pin for the noise sensor
#define NOISE_PIN   D3 // digital input pin for the noise sensor
#define MOTION_PIN  D2 // input pin for the motion PIR sensor

//comment out to completly disable respective technology
//#define INCLUDE_BLYNK_SUPPORT
#define INCLUDE_MQTT_SUPPORT

/********************************************
   Should not need to edit below this line *
 * *****************************************/
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <EEPROM.h>
#include <Ticker.h>

#define EEPROM_SALT 12657

typedef struct {
  char  bootState[4]      = "off";
  char  blynkToken[33]    = "";
  char  blynkServer[33]   = "blynk-cloud.com";
  char  blynkPort[6]      = "9442";
  char  mqttHostname[33]  = "192.168.100.3";
  char  mqttPort[6]       = "1883";
  char  mqttClientID[24]  = "ESP8266Client";
  char  mqttTopic[33]     = HOSTNAME;
  int   salt              = EEPROM_SALT;
} WMSettings;

WMSettings settings;

Ticker ticker; //for LED status

const int CMD_WAIT = 0;
const int CMD_BUTTON_CHANGE = 1;

int cmd = CMD_WAIT;

int buttonState = HIGH; //inverted button state
static long startPress = 0;

bool shouldSaveConfig = false; //flag for saving data

//http://stackoverflow.com/questions/9072320/split-string-into-string-array
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void tick()
{
  //toggle state
  int state = digitalRead(SONOFF_LED);  // get the current state of GPIO1 pin
  digitalWrite(SONOFF_LED, !state);     // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  //entered config mode, make led toggle faster
  ticker.attach(0.2, tick);
}

void setState(int state, int channel) {
  //relay
  digitalWrite(SONOFF_RELAY_PINS[channel], state);

  //led
  if (SONOFF_LED_RELAY_STATE) {
    digitalWrite(SONOFF_LED, (state + 1) % 2); // led is active low
  }

#ifdef INCLUDE_BLYNK_SUPPORT
  Blynk_update(channel);
#endif

#ifdef INCLUDE_MQTT_SUPPORT
  MQTT_update(channel);
#endif
}

void turnOn(int channel = 0) {
  int relayState = HIGH;
  setState(relayState, channel);
}

void turnOff(int channel = 0) {
  int relayState = LOW;
  setState(relayState, channel);
}

void toggleState() {
  cmd = CMD_BUTTON_CHANGE;
}

#ifdef PRESENCE_SENSOR

int presenceState = LOW;

static int noiseState = LOW;
static long startNoise = 0; // noise detection start time
static int motionState = LOW;
static long startMotion = 0; // noise detection start time
static long waiting = 0; // turn off start time

void presence_loop() {
  //Serial.println(analogRead(NOISE_LEVEL));

  // if no presence - wait 5 minutes
  if (!presenceState) {
    long duration = millis() - waiting;
    if (duration > 5 * 60 * 1000) {
      ticker.detach();
      turnOff(0);
    }
  }
  if (noiseState) {
    long duration = millis() - startNoise;
    if (duration > 3000) {
      noiseState = LOW;
    }
  }
  int newState = noiseState | motionState;
  if (presenceState != newState) {
    if (newState) {
      Serial.println("Presence detected");
      ticker.detach();
      turnOn(0);
    } else {
      Serial.println("No presence");
      waiting = millis();
      ticker.attach(0.6, tick);
      //turnOff(0);
    }
    presenceState = newState;
  }
}

void noiseDetected() {
  noiseState = HIGH;
  startNoise = millis();
}
void motionDetected() {
  motionState = digitalRead(MOTION_PIN);
  if (motionState) {
    startMotion = millis();
  }
}
#endif

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void toggle(int channel = 0) {
  Serial.println("toggle state");
  Serial.println(digitalRead(SONOFF_RELAY_PINS[channel]));
  int relayState = digitalRead(SONOFF_RELAY_PINS[channel]) == HIGH ? LOW : HIGH;
  setState(relayState, channel);
}

void restart() {
  //TODO turn off relays before restarting
  ESP.reset();
  delay(1000);
}

void reset() {
  //reset settings to defaults
  //TODO turn off relays before restarting
  /*
    WMSettings defaults;
    settings = defaults;
    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.end();
  */
  //reset wifi credentials
  WiFi.disconnect();
  delay(1000);
  ESP.reset();
  delay(1000);
}

void setup_wifi() {
  const char *hostname = HOSTNAME;

  //custom params
  EEPROM.begin(512);
  EEPROM.get(0, settings);
  EEPROM.end();

  if (settings.salt != EEPROM_SALT) {
    Serial.println("Invalid settings in EEPROM, trying with defaults");
    WMSettings defaults;
    settings = defaults;
  }

  WiFiManager wifiManager;
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);

  //timeout - this will quit WiFiManager if it's not configured in 3 minutes, causing a restart
  wifiManager.setConfigPortalTimeout(180);

  WiFiManagerParameter custom_boot_state("boot-state", "on/off on boot", settings.bootState, 33);
  wifiManager.addParameter(&custom_boot_state);
  Serial.println(settings.bootState);

#ifdef INCLUDE_BLYNK_SUPPORT
  WiFiManagerParameter custom_blynk_text("<br/>Blynk config. <br/> No token to disable.<br/>");
  wifiManager.addParameter(&custom_blynk_text);

  WiFiManagerParameter custom_blynk_token("blynk-token", "blynk token", settings.blynkToken, 33);
  wifiManager.addParameter(&custom_blynk_token);
  Serial.println(settings.blynkToken);

  WiFiManagerParameter custom_blynk_server("blynk-server", "blynk server", settings.blynkServer, 33);
  wifiManager.addParameter(&custom_blynk_server);
  Serial.println(settings.blynkServer);

  WiFiManagerParameter custom_blynk_port("blynk-port", "port", settings.blynkPort, 6);
  wifiManager.addParameter(&custom_blynk_port);
  Serial.println(settings.blynkServer);
#endif


#ifdef INCLUDE_MQTT_SUPPORT
  WiFiManagerParameter custom_mqtt_text("<br/>MQTT config. <br/> No url to disable.<br/>");
  wifiManager.addParameter(&custom_mqtt_text);

  WiFiManagerParameter custom_mqtt_hostname("mqtt-hostname", "Hostname", settings.mqttHostname, 33);
  wifiManager.addParameter(&custom_mqtt_hostname);
  Serial.println(settings.mqttHostname);

  WiFiManagerParameter custom_mqtt_port("mqtt-port", "port", settings.mqttPort, 6);
  wifiManager.addParameter(&custom_mqtt_port);
  Serial.println(settings.mqttPort);

  WiFiManagerParameter custom_mqtt_client_id("mqtt-client-id", "Client ID", settings.mqttClientID, 24);
  wifiManager.addParameter(&custom_mqtt_client_id);
  Serial.println(settings.mqttClientID);

  WiFiManagerParameter custom_mqtt_topic("mqtt-topic", "Topic", settings.mqttTopic, 33);
  wifiManager.addParameter(&custom_mqtt_topic);
  Serial.println(settings.mqttTopic);
#endif

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  if (!wifiManager.autoConnect(hostname)) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("Saving config");

    strcpy(settings.bootState, custom_boot_state.getValue());
    Serial.println(settings.bootState);

#ifdef INCLUDE_BLYNK_SUPPORT
    strcpy(settings.blynkToken, custom_blynk_token.getValue());
    Serial.println(settings.blynkToken);

    strcpy(settings.blynkServer, custom_blynk_server.getValue());
    Serial.println(settings.blynkServer);

    strcpy(settings.blynkPort, custom_blynk_port.getValue());
    Serial.println(settings.blynkPort);
#endif

#ifdef INCLUDE_MQTT_SUPPORT
    strcpy(settings.mqttHostname, custom_mqtt_hostname.getValue());
    strcpy(settings.mqttPort, custom_mqtt_port.getValue());
    strcpy(settings.mqttClientID, custom_mqtt_client_id.getValue());
    strcpy(settings.mqttTopic, custom_mqtt_topic.getValue());
#endif

    EEPROM.begin(512);
    EEPROM.put(0, settings);
    EEPROM.end();
  }
}

void setup()
{
  Serial.begin(115200);

  //set led pin as output
  pinMode(SONOFF_LED, OUTPUT);
  // start ticker with 0.5 because we start in AP mode and try to connect
  ticker.attach(0.6, tick);

  setup_wifi();

#ifdef INCLUDE_BLYNK_SUPPORT
  BLYNK_setup();
#endif

#ifdef INCLUDE_MQTT_SUPPORT
  MQTT_setup();
#endif

  const char *hostname = HOSTNAME;

  //OTA
  ArduinoOTA.onStart([]() {
    Serial.println("Start OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    delay(5000);
    ESP.restart();
  });
  ArduinoOTA.setHostname(hostname);
  ArduinoOTA.begin();

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");
  ticker.detach();

  //setup button
  pinMode(SONOFF_BUTTON, INPUT);
  attachInterrupt(SONOFF_BUTTON, toggleState, CHANGE);

  //setup relay
  for (int n = 0; n < SONOFF_AVAILABLE_CHANNELS; n++) {
    pinMode(SONOFF_RELAY_PINS[n], OUTPUT);
  }
  //TODO this should move to last state maybe
  //TODO multi channel support
  if (strcmp(settings.bootState, "on") == 0) {
    turnOn();
  } else {
    turnOff();
  }

  //setup led
  if (!SONOFF_LED_RELAY_STATE) {
    digitalWrite(SONOFF_LED, LOW);
  }

#ifdef PRESENCE_SENSOR
  pinMode(NOISE_LEVEL, INPUT);

  pinMode(NOISE_PIN, INPUT);
  //attachInterrupt(digitalPinToInterrupt(NOISE_PIN), noiseDetected, RISING);
  attachInterrupt(NOISE_PIN, noiseDetected, RISING);

  pinMode(MOTION_PIN, INPUT);
  //attachInterrupt(digitalPinToInterrupt(MOTION_PIN), motionDetected, CHANGE);
  attachInterrupt(MOTION_PIN, motionDetected, CHANGE);
#endif

  Serial.println("done setup");
}

void loop()
{
  //ota loop
  ArduinoOTA.handle();

#ifdef INCLUDE_BLYNK_SUPPORT
  Blynk_loop();
#endif


#ifdef INCLUDE_MQTT_SUPPORT
  MQTT_loop();
#endif

#ifdef PRESENCE_SENSOR
  presence_loop();
#endif

  switch (cmd) {
    case CMD_WAIT:
      break;
    case CMD_BUTTON_CHANGE:
      int currentState = digitalRead(SONOFF_BUTTON);
      if (currentState != buttonState) {
        if (buttonState == LOW && currentState == HIGH) {
          long duration = millis() - startPress;
          if (duration < 1000) {
            Serial.println("short press - toggle relay");
            toggle();
          } else if (duration < 5000) {
            Serial.println("medium press - reset");
            restart();
          } else if (duration < 60000) {
            Serial.println("long press - reset settings");
            reset();
          }
        } else if (buttonState == HIGH && currentState == LOW) {
          startPress = millis();
        }
        buttonState = currentState;
      }
      break;
  }
  cmd = CMD_WAIT;

  //  yield();     // yield == delay(0), delay contains yield, auto yield in loop
  delay(0);  // https://github.com/esp8266/Arduino/issues/2021
}




