#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <EEPROM.h>
#include <Ticker.h>

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

//if this is false, led is used to signal startup state, then always on
//if it s true, it is used to signal startup state, then mirrors relay state
//S20 Smart Socket works better with it false
#define SONOFF_LED_RELAY_STATE      true

#define HOSTNAME               "sonoff"
#define WIFI_HOSTNAME          HOSTNAME "-%04d"  // Expands to <HOSTNAME>-<last 4 decimal chars of MAC address>
#define INPUT_BUFFER_SIZE      250               // Max number of characters in (serial) command buffer
#define CMDSZ                  20                // Max number of characters in command
#define TOPSZ                  100               // Max number of characters in topic string

#define NOISE_PIN   D0 // digital input pin for the noise sensor
#define MOTION_PIN  D5 // input pin for the motion PIR sensor

// -- BLYNK ---------------------------------------
#define USE_BLYNK                                // comment out to completly disable respective technology

#define BLYNK_SERVER           "blynk-cloud.com" // [BlynkHost]
#define BLYNK_PORT             8442              // [BlynkPort] BLYNK port
#define BLYNK_TOKEN            ""                // [BlynkToken] (unique) Blynk token

// -- MQTT ----------------------------------------
#define USE_MQTT                                 // comment out to completly disable respective technology

#define MQTT_HOST              "192.168.100.3"   // [MqttHost]
#define MQTT_PORT              1883              // [MqttPort] MQTT port (10123 on CloudMQTT)
#define MQTT_TOPIC             HOSTNAME          // [Topic] (unique) MQTT device topic
#define MQTT_CLIENT_ID         "ESP8266_%06X"    // [MqttClient] Also fall back topic using Chip Id = last 6 characters of MAC address

#define SUB_PREFIX             "cmnd"            // [Prefix1] Sonoff devices subscribe to %prefix%/%topic% being SUB_PREFIX/MQTT_TOPIC and SUB_PREFIX/MQTT_GRPTOPIC
#define PUB_PREFIX             "stat"            // [Prefix2] Sonoff devices publish to %prefix%/%topic% being PUB_PREFIX/MQTT_TOPIC
#define PUB_PREFIX2            "tele"            // [Prefix3] Sonoff devices publish telemetry data to %prefix%/%topic% being PUB_PREFIX2/MQTT_TOPIC/UPTIME, POWER and TIME

// -- HTTP ----------------------------------------
#define USE_WEBSERVER                            // Enable web server and wifi manager (+62k code, +8k mem) - Disable by //

#define WEB_SERVER             2                 // [WebServer] Web server (0 = Off, 1 = Start as User, 2 = Start as Admin)
#define WEB_PORT               80                // Web server Port for User and Admin mode

// -- mDNS ----------------------------------------
#define USE_DISCOVERY                            // Enable mDNS for the following services (+8k code, +0.3k mem) - Disable by //

#define WEBSERVER_ADVERTISE                      // Provide access to webserver by name <Hostname>.local/
#define MQTT_HOST_DISCOVERY                      // Find MQTT host server (overrides MQTT_HOST if found)

#define DEBOUNCE               50                // Debounce time for buttons and sensors

/********************************************
   Should not need to edit below this line *
 * *****************************************/

#define MQTT_STATUS_OFF        "OFF"             // [StateText1] Command or Status result when turned off (needs to be a string like "0" or "Off")
#define MQTT_STATUS_ON         "ON"              // [StateText2] Command or Status result when turned on (needs to be a string like "1" or "On")

#define MAX_RELAY              4
#define MAX_SENSOR             4
#define EEPROM_SALT            12657

enum sensor_t { NO_SENSOR, MOTION_SENSOR, SOUND_SENSOR, SENSOR_END };

typedef struct {
	// common
	int  module = 0;
	char bootState[4] = MQTT_STATUS_OFF;
	char hostname[33] = WIFI_HOSTNAME;
	char blynkToken[33] = BLYNK_TOKEN;
	char blynkServer[33] = BLYNK_SERVER;
	int  blynkPort = BLYNK_PORT;
	char mqttHost[33] = MQTT_HOST;
	int  mqttPort = MQTT_PORT;
	char mqttClientID[24] = MQTT_CLIENT_ID;
	char mqttTopic[33] = HOSTNAME;

	// basic
	int buttonPin = 0;             // button pin
	int ledPin = 13;               // led pin
	bool ledInverted = true;       // If true - LOW for led on, otherwise HIGH for led on

	// relays
	int relayPins[MAX_RELAY] = { 12, NOT_A_PIN, NOT_A_PIN, NOT_A_PIN };

	// sensors
	int sensorPins[MAX_SENSOR] = { 14, NOT_A_PIN, NOT_A_PIN, NOT_A_PIN };
	int sensorModes[MAX_SENSOR] = { NO_SENSOR, NO_SENSOR, NO_SENSOR, NO_SENSOR };
	int relayHoldTime = 0;        // Relay hold on time in seconds

	// salt
	int  salt = EEPROM_SALT;
} WMSettings;

WMSettings settings;

Ticker ticker; //for LED status

char Hostname[33];                    // Composed Wifi hostname
boolean mDNSbegun = false;
int restartflag = 0;                  // Sonoff restart flag
int MaxRelay;
int powerState = LOW;                 // if there is no relay, we use a variable to store the status
bool analog_flag = false;             // Analog input enabled
int sensorStates[MAX_SENSOR] =        // Current sensor states
{
	LOW,
	LOW,
	LOW,
	LOW 
};
int onholdrelay = 0;                  // relay on hold counter
unsigned long timerloop = 0;          // 0.1 sec loop timer
int countloop = 0;                    // loop counter

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
	int strIndex[] = { 0, -1 };
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

void getClient(char* output, const char* input, byte size)
{
	char *token;
	uint8_t digits = 0;

	if (strstr(input, "%")) {
		strlcpy(output, input, size);
		token = strtok(output, "%");
		if (strstr(input, "%") == input) {
			output[0] = '\0';
		}
		else {
			token = strtok(NULL, "");
		}
		if (token != NULL) {
			digits = atoi(token);
			if (digits) {
				snprintf_P(output, size, PSTR("%s%c0%dX"), output, '%', digits);
				snprintf_P(output, size, output, ESP.getChipId());
			}
		}
	}
	if (!digits) {
		strlcpy(output, input, size);
	}
}

char* getStateText(int state)
{
	if (state) {
		return MQTT_STATUS_ON;
	}
	return MQTT_STATUS_OFF;
}

void tick()
{
	//toggle state
	int state = digitalRead(settings.ledPin);  // get the current state of led pin
	digitalWrite(settings.ledPin, !state);     // set pin to the opposite state
}

void toggleState() {
	cmd = CMD_BUTTON_CHANGE;
}


//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *wiFiManager) {
	Serial.println("Entered config mode");
	Serial.println(WiFi.softAPIP());
	//if you used auto generated SSID, print it
	Serial.println(wiFiManager->getConfigPortalSSID());
	//entered config mode, make led toggle faster
	ticker.attach(0.2, tick);
}

//callback notifying us of the need to save config
void saveConfigCallback() {
	Serial.println("Should save config");
	shouldSaveConfig = true;
}


void setRelay(int power, int relay) {
	//relay
	if (MaxRelay) {
		digitalWrite(settings.relayPins[relay - 1], power);
	}
	else {
		powerState = power;
	}

	//led
	setLed(power);

#ifdef USE_BLYNK
	BLYNK_update(relay);
#endif

#ifdef USE_MQTT
	MQTT_update(relay);
#endif
}

void setLed(int state) {
	if (SONOFF_LED_RELAY_STATE) {
		digitalWrite(settings.ledPin, (state + (settings.ledInverted ? 1 : 0)) % 2); // if inverted - led active is low
	}
}

void do_cmnd_power(int relay, byte state)
{
	// device  = Relay number 1 and up
	// state 0 = Relay Off
	// state 1 = Relay On
	// state 2 = Toggle relay
	// state 2 = Hold On relay 

	if ((relay < 1) || (relay > MaxRelay)) {
		relay = 1;
	}
	int power = MaxRelay ? digitalRead(settings.relayPins[relay - 1]) : powerState;
	switch (state) {
	case 0:  // Off
		Serial.print("turn off ");
		power = LOW;
		onholdrelay = 0; // disable hold on
		break;
	case 1: // On
		Serial.print("turn on ");
		power = HIGH;
		onholdrelay = 0; // disable hold on
		break;
	case 2: // Toggle
		Serial.print("toggle state ");
		power = power == HIGH ? LOW : HIGH;
		onholdrelay = 0; // disable hold on
		break;
	case 3: // Hold On
		Serial.print("hold on ");
		if (settings.relayHoldTime) {
			// if hold on enabled
			if (!power) {
				setRelay(HIGH, relay); // set high power if is not
			}
			onholdrelay = settings.relayHoldTime;
		}
		else {
			// no hold time, simple turn off
			power = LOW;
		}
	}
	Serial.println(relay);
	if (!onholdrelay) {
		setRelay(power, relay);
	}
}

void do_cmnd(char *cmnd)
{
	char stopic[CMDSZ];
	char svalue[INPUT_BUFFER_SIZE];
	char *start;
	char *token;

	token = strtok(cmnd, " ");
	if (token != NULL) {
		start = strrchr(token, '/');   // Skip possible cmnd/sonoff/ preamble
		if (start) {
			token = start + 1;
		}
	}
	snprintf_P(stopic, sizeof(stopic), PSTR("/%s"), (token == NULL) ? "" : token);
	token = strtok(NULL, "");
	snprintf_P(svalue, sizeof(svalue), PSTR("%s"), (token == NULL) ? "" : token);
	mqttDataCb(stopic, (byte*)svalue, strlen(svalue));
}


void cfg_load() {
	EEPROM.begin(512);
	EEPROM.get(0, settings);
	EEPROM.end();

	if (settings.salt != EEPROM_SALT) {
		Serial.println("Invalid settings in EEPROM, trying with defaults");
		WMSettings defaults;
		settings = defaults;
	}

	// validate hostname from setting and initialize composed Wifi hostname
	if (strstr(settings.hostname, "%")) {
		strlcpy(settings.hostname, WIFI_HOSTNAME, sizeof(settings.hostname));
		snprintf_P(Hostname, sizeof(Hostname) - 1, settings.hostname, ESP.getChipId() & 0x1FFF);
	}
	else {
		snprintf_P(Hostname, sizeof(Hostname) - 1, settings.hostname);
	}

	MaxRelay = 0; // initialize MaxRelay
	for (byte idx = 0; idx < MAX_RELAY; idx++)
	{
		if (settings.relayPins[idx] != NOT_A_PIN) {
			MaxRelay++;
		}
	}
}

void cfg_save() {
	EEPROM.begin(512);
	EEPROM.put(0, settings);
	EEPROM.end();
}

// reset settings to defaults
void cfg_reset() {
	WMSettings defaults;
	EEPROM.begin(512);
	EEPROM.put(0, defaults);
	EEPROM.end();
}


void restart() {
	Serial.println("APP: Restarting");
	for (byte idx = 1; idx <= MaxRelay; idx++) {
		do_cmnd_power(idx, 0);
	}
	//ESP.restart();
	WiFi.forceSleepBegin(); wdt_reset(); ESP.restart(); while (1)wdt_reset();
}

void reset() {
	Serial.println("APP: Reseting parameters to default");
	cfg_reset();
	//reset wifi credentials
	WiFi.disconnect();
	delay(1000);
}

void setup_wifi() {
	WiFiManager wifiManager;
	//set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
	wifiManager.setAPCallback(configModeCallback);

	//timeout - this will quit WiFiManager if it's not configured in 3 minutes, causing a restart
	wifiManager.setConfigPortalTimeout(180);

	WiFiManagerParameter custom_hostname_text("<br/><b>Hostname</b> (" WIFI_HOSTNAME ")<br/>");
	wifiManager.addParameter(&custom_hostname_text);

	WiFiManagerParameter custom_hostname("hostname", WIFI_HOSTNAME, settings.hostname, sizeof(settings.hostname));
	wifiManager.addParameter(&custom_hostname);
	Serial.println(settings.hostname);

	WiFiManagerParameter custom_bootState_text("<br/><b>Relay boot state</b> (" MQTT_STATUS_OFF ")<br/>");
	wifiManager.addParameter(&custom_bootState_text);

	WiFiManagerParameter custom_bootState("bootState", MQTT_STATUS_OFF, settings.bootState, sizeof(settings.bootState));
	wifiManager.addParameter(&custom_bootState);
	Serial.println(settings.bootState);

#ifdef PRESENCE_SENSOR
	WiFiManagerParameter custom_presence_sensor_text("<br/>Presence sensor config.<br/>");
	wifiManager.addParameter(&custom_presence_sensor_text);

	WiFiManagerParameter custom_presence_sensor_delay("presence-sensor-delay", "Delay in minutes (0=off)", String(settings.presenceSensorDelay / 60 / 1000).c_str(), 3);
	wifiManager.addParameter(&custom_presence_sensor_delay);
	Serial.println(settings.presenceSensorDelay);

#endif

	//set config save notify callback
	wifiManager.setSaveConfigCallback(saveConfigCallback);

	if (!wifiManager.autoConnect(Hostname)) {
		Serial.println("failed to connect and hit timeout");
		//reset and try again, or maybe put it to deep sleep
		ESP.reset();
		delay(1000);
	}

	//save the custom parameters to FS
	if (shouldSaveConfig) {
		Serial.println("Saving config");

		strcpy(settings.hostname, custom_hostname.getValue());

		// validate hostname from setting and initialize composed Wifi hostname
		if (strstr(settings.hostname, "%")) {
			strlcpy(settings.hostname, WIFI_HOSTNAME, sizeof(settings.hostname));
			snprintf_P(Hostname, sizeof(Hostname) - 1, settings.hostname, ESP.getChipId() & 0x1FFF);
		}
		else {
			snprintf_P(Hostname, sizeof(Hostname) - 1, settings.hostname);
		}

		String str = String(custom_bootState.getValue());
		str.toUpperCase();
		if (strcmp(str.c_str(), MQTT_STATUS_ON)) {
			strcpy(settings.bootState, MQTT_STATUS_ON);
		}
		else {
			strcpy(settings.bootState, MQTT_STATUS_OFF);
		}
		Serial.println(settings.bootState);

#ifdef PRESENCE_SENSOR
		settings.presenceSensorDelay = atol(custom_presence_sensor_delay.getValue());
#endif

		cfg_save();
	}

	WiFi.hostname(Hostname);

#ifdef USE_DISCOVERY
	if (!mDNSbegun) {
		mDNSbegun = MDNS.begin(Hostname);
	}
#endif // USE_DISCOVERY
#ifdef USE_WEBSERVER
	startWebserver(WEB_SERVER);
#ifdef USE_DISCOVERY
#ifdef WEBSERVER_ADVERTISE
	MDNS.addService("http", "tcp", 80);
#endif  // WEBSERVER_ADVERTISE          
#endif  // USE_DISCOVERY
#endif  // USE_WEBSERVER
}

void setup()
{
	Serial.begin(115200);

	// load settings
	cfg_load();

	//set led pin as output
	pinMode(settings.ledPin, OUTPUT);
	// start ticker with 0.5 because we start in AP mode and try to connect
	ticker.attach(0.6, tick);

	setup_wifi();

#ifdef USE_BLYNK
	BLYNK_setup();
#endif

#ifdef USE_MQTT
	MQTT_setup();
#endif

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
	ArduinoOTA.setHostname(Hostname);
	ArduinoOTA.begin();

	//if you get here you have connected to the WiFi
	Serial.println("connected...yeey :)");
	ticker.detach();

	//setup button
	Serial.printf("Button pin: %02d\r\n", settings.buttonPin);
	pinMode(settings.buttonPin, INPUT);
	attachInterrupt(settings.buttonPin, toggleState, CHANGE);

	//setup relay
	if (MaxRelay) {
		for (byte idx = 1; idx <= MaxRelay; idx++) {
			pinMode(settings.relayPins[idx - 1], OUTPUT);
			if (strcmp(settings.bootState, MQTT_STATUS_ON) == 0) {
				do_cmnd_power(idx, 1);
			}
			else {
				do_cmnd_power(idx, 0);
			}
		}
	}
	else {
		if (strcmp(settings.bootState, MQTT_STATUS_ON) == 0) {
			do_cmnd_power(1, 1);
		}
		else {
			do_cmnd_power(1, 0);
		}
	}

	sensor_setup();

	Serial.println("done setup");
}

// every second
void every_second() {
	// check if on hold
	if (onholdrelay) {
		onholdrelay--;

		// blinks led
		setLed(onholdrelay % 2);

		if (!onholdrelay) {
			// turn off relay
			do_cmnd_power(1, 0);
		}
	}

	sensor_publish();
}

void loop()
{
	if (millis() > timerloop) {
		// every 0.1 second
		timerloop = millis() + 100;
		countloop++;

		if (countloop == 10) {
			// every second
			countloop = 0;
			every_second();
		}

		if (restartflag) {
			if (200 == restartflag) {
				reset();
				restartflag = 2;
			}
			restartflag--;
			if (restartflag <= 0) {
				restart();
			}
		}

		sensor_state();

		if (!(countloop % 2)) {
			// every 0.2 second
			sensor_publish();
		}
	}

	//ota loop
	ArduinoOTA.handle();

#ifdef USE_WEBSERVER
	pollDnsWeb();
#endif // USE_WEBSERVER

#ifdef USE_BLYNK
	BLYNK_loop();
#endif

#ifdef USE_MQTT
	MQTT_loop();
#endif

	switch (cmd) {
	case CMD_WAIT:
		break;
	case CMD_BUTTON_CHANGE:
		int currentState = digitalRead(settings.buttonPin);
		if (currentState != buttonState) {
			if (buttonState == LOW && currentState == HIGH) {
				long duration = millis() - startPress;
				if (duration < DEBOUNCE) {
					// bounce
				}
				else if (duration < 1000) {
					Serial.println("short press - toggle relay");
					do_cmnd_power(1, 2);
				}
				else if (duration < 5000) {
					Serial.println("medium press - restart");
					restartflag = 2;
				}
				else if (duration < 60000) {
					Serial.println("long press - reset settings");
					restartflag = 200;
				}
			}
			else if (buttonState == HIGH && currentState == LOW) {
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




