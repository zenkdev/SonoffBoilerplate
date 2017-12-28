/*********************************************/
//   1MB flash sizee
//
//   sonoff header
//   1 - vcc 3v3
//   2 - rx
//   3 - tx
//   4 - gnd
//   5 - gpio 14
//
//   esp8266 connections
//   gpio  0 - button
//   gpio 12 - relay
//   gpio 13 - green led - active low
//   gpio 14 - pin 5 on header
/*********************************************/

#include <ESP8266WiFi.h>

enum wifi_t { WIFI_RESTART, WIFI_SMARTCONFIG, WIFI_MANAGER, WIFI_WPSCONFIG, WIFI_RETRY, WIFI_WAIT, MAX_WIFI_OPTION };
enum http_t { HTTP_OFF, HTTP_USER, HTTP_ADMIN, HTTP_MANAGER };
enum sensor_t { NO_SENSOR, MOTION_SENSOR, SOUND_SENSOR, SENSOR_END };
enum button_t { PRESSED, NOT_PRESSED };
enum ledmode_t { LED_OFF, LED_SYSTEM, LED_SYSTEM_RELAY, LED_SYSTEM_SENSOR, MAX_LED_OPTION };


#include "config.h"

#ifdef USE_BLYNK
#define BLYNK_PRINT            Serial  // Comment this out to disable prints and save space
#include <BlynkSimpleEsp8266.h>

uint8_t blynk_counter = 0;             // BLYNK retry counter
boolean blynk_enabled = true;
WidgetLED* sensorLED[MAX_SENSOR];

#endif // USE_BLYNK

#ifdef USE_MQTT

#include <PubSubClient.h>

#if (MQTT_MAX_PACKET_SIZE -TOPSZ -7) < MESSZ  // If the max message size is too small, throw an error at compile time
// See pubsubclient.c line 359
#error "MQTT_MAX_PACKET_SIZE is too small in libraries/PubSubClient/src/PubSubClient.h, increase it to at least 467"
#endif

WiFiClient espClient;
PubSubClient mqttClient(espClient);

char mqttClientID[33];                 // Composed MQTT ClientID
uint8_t mqtt_counter = 0;              // MQTT retry counter
boolean mqtt_enabled = true;

#endif // USE_MQTT

#ifdef USE_WEBSERVER

#include <ESP8266WebServer.h>          // WiFiManager
#include <DNSServer.h>                 // WiFiManager
//#include <WiFiManager.h>               // https://github.com/tzapu/WiFiManager

#endif // USE_WEBSERVER

#include <EEPROM.h>
#include <ESP8266mDNS.h>               // ArduinoOTA
#include <ArduinoOTA.h>

WMSettings settings;
char Hostname[33];                    // Composed Wifi hostname
boolean mDNSbegun = false;

int restart_flag = 0;                 // Sonoff restart flag
int wificheckflag = WIFI_RESTART;     // Wifi state flag
uint8_t power_state = LOW;            // if there is no relay, we use a variable to store the status
byte MaxRelay;

boolean analog_flag = false;          // Analog input enabled
int sensor_state[MAX_SENSOR] = { LOW, LOW, LOW, LOW }; // Current sensor states

ulong loop_timer = 0;                 // 0.1 sec loop timer
byte loop_count = 0;                  // loop counter
int16_t hold_count = 0;               // hold counter

uint8_t lastbutton = NOT_PRESSED;     // Last button states
uint8_t holdbutton = 0;               // Timer for button hold
uint8_t multiwindow = 0;              // Max time between button presses to record press count
uint8_t multipress = 0;               // Number of button presses within multiwindow

int led_blinks = 2;                   // Number of LED blinks
uint8_t led_state = 0;                // LED state

int tele_period = 0;                  // Tele period timer
uint16_t seconds_counter = 0;         // Seconds counter for uptime
int uptime = 0;                       // Current uptime in hours


void setup() {
	Serial.begin(115200);

	cfg_load(); // load settings

	pinMode(settings.led_pin, OUTPUT); // Initialize the BUILTIN_LED pin as an output

	WIFI_Connect();

#ifdef USE_BLYNK
	if (strlen(settings.blynk_token) == 0) {
		blynk_enabled = false;
	}
	else {
		Blynk.config(settings.blynk_token, settings.blynk_server, settings.blynk_port);
		for (byte i = 0; i < MAX_SENSOR; i++) {
			if (settings.sensor_mode[i] != NO_SENSOR) {
				sensorLED[i] = new WidgetLED(BLYNK_ANALOG_PIN + 1 + i);
			}
		}
	}
#endif // USE_BLYNK

#ifdef USE_MQTT
	if (strlen(settings.mqtt_host) == 0) {
		mqtt_enabled = false;
	}
	else {
		getClient(mqttClientID, settings.mqtt_client_id, sizeof(mqttClientID));
#ifdef USE_DISCOVERY
#ifdef MQTT_HOST_DISCOVERY
		mdns_discoverMQTTServer();
#endif  // MQTT_HOST_DISCOVERY
#endif  // USE_DISCOVERY
		mqttClient.setServer(settings.mqtt_host, settings.mqtt_port);
		mqttClient.setCallback(mqtt_data_cb);
	}
#endif // USE_MQTT

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

	//setup button
	pinMode(settings.button_pin, INPUT);

	//setup relay
	if (MaxRelay) {
		for (byte idx = 0; idx < MaxRelay; idx++) {
			pinMode(settings.relay_pin[idx], OUTPUT);
		}
	}

	typedef void(*function) ();
	function sensor_callback[] = { sensor_update1, sensor_update2, sensor_update3, sensor_update4 };

	for (byte i = 0; i < MAX_SENSOR; i++) {
		if (settings.sensor_mode[i] != NO_SENSOR) {
			pinMode(settings.sensor_pin[i], INPUT);
			attachInterrupt(settings.sensor_pin[i], sensor_callback[i], RISING);
		}
		if (settings.sensor_mode[i] == SOUND_SENSOR) {
			analog_flag = true;
			pinMode(A0, INPUT);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// Button handler with single press only or multi-press and hold on all buttons
void button_handler()
{
	uint8_t button = NOT_PRESSED;
	uint8_t flag = 0;
	char scmnd[20];

	button = digitalRead(settings.button_pin);

	if ((PRESSED == button) && (NOT_PRESSED == lastbutton)) {
		multipress = (multiwindow) ? multipress + 1 : 1;
		Serial.printf("BTN: Multi-press button %d\n", multipress);
		multiwindow = 5; // 0.5 second multi press window
	}

	if (NOT_PRESSED == button) {
		holdbutton = 0;
	}
	else {
		holdbutton++;
		if (holdbutton == 40) {      // Button hold 4 seconds
			multipress = 0;
			Serial.println("BTN: Button hold 4 seconds - restart");
			restart_flag = 200;
		}
	}

	if (multiwindow) {
		multiwindow--;
	}
	else {
		if (!restart_flag && !holdbutton && (multipress > 0) && (multipress < MAX_BUTTON_COMMANDS + 3)) {
			flag = 0;
			if (multipress < 3) {                  // Single or Double press
				flag = (2 == multipress);
				multipress = 1;
			}
			if (multipress < 3) {                  // Single or Double press
				if (WIFI_State()) {                // WPSconfig, Smartconfig or Wifimanager active
					restart_flag = 1;
				}
				else {
					do_cmnd_power(multipress, 2);  // Execute Toggle command internally
				}
			}
			else {                                 // 3 - 7 press
				snprintf_P(scmnd, sizeof(scmnd), commands[multipress - 3]);
				do_cmnd(scmnd);
			}
			multipress = 0;
		}
	}
	lastbutton = button;
}

void loop()
{
	if (millis() > loop_timer) {
		// every 0.1 second
		loop_timer = millis() + 100;
		loop_count++;

		button_handler();

		// every 0.2 second
		if (!(loop_count % 2)) {
			if (led_blinks || restart_flag) {
				if (restart_flag) {
					led_state = 1;   // Stay lit
				}
				else {
					led_state ^= 1;  // Blink
				}
				led_set(led_state);
				if (!led_state) {
					//Serial.printf("LED: Blinks count %d\n", led_blinks);
					led_blinks--;
				}
			}
		}

		if (loop_count == 10) {
			// every second
			loop_count = 0;
			every_second();
		}

		if (restart_flag) {
			if (200 == restart_flag) {
				reset();
				restart_flag = 2;
			}
			restart_flag--;
			if (restart_flag <= 0) {
				restart();
			}
		}

		sensor_checkState();
	}

#ifdef USE_WEBSERVER
	pollDnsWeb();
#endif // USE_WEBSERVER

	if (WiFi.status() == WL_CONNECTED) {
		//ota loop
		ArduinoOTA.handle();

#ifdef USE_BLYNK
		if (blynk_enabled) {
			Blynk.run();
		}
#endif // USE_BLYNK

#ifdef USE_MQTT
		if (mqtt_enabled) {
			mqttClient.loop();
		}
#endif // USE_MQTT
	}

	//  yield();     // yield == delay(0), delay contains yield, auto yield in loop
	delay(0);  // https://github.com/esp8266/Arduino/issues/2021
}

void relay_set(uint8_t power, int relay) {
	//relay
	if (MaxRelay) {
		digitalWrite(settings.relay_pin[relay - 1], power);
	}
	else {
		power_state = power;
	}

	// led control
	if (settings.led_mode == LED_SYSTEM_RELAY) {
		led_set(power);
	}

#ifdef USE_BLYNK
	if (blynk_enabled) {
		update_blynk(relay);
	}
#endif

#ifdef USE_MQTT
	if (mqtt_enabled) {
		update_mqtt(relay);
	}
#endif
}

void led_set(uint8_t state) {
	if (state) {
		state = 1;
	}
	digitalWrite(settings.led_pin, settings.led_inverted ? !state : state); // if inverted - led active is low
	//Serial.printf("LED: Set state %d\n", state);
}

void do_cmnd_power(int relay, byte state) {
	// device  = Relay number 1 and up
	// state 0 = Relay Off
	// state 1 = Relay On
	// state 2 = Toggle relay
	// state 3 = Hold relay

	if ((relay < 1) || (relay > MaxRelay)) {
		relay = 1;
	}
	int power = MaxRelay ? digitalRead(settings.relay_pin[relay - 1]) : power_state;
	switch (state) {
	case 0:  // Off
		power = LOW;
		hold_count = 0; // disable hold on
		Serial.printf("PWR: Turn off, relay %d\n", relay);
		break;
	case 1: // On
		power = HIGH;
		hold_count = 0; // disable hold on
		Serial.printf("PWR: Turn on, relay %d\n", relay);
		break;
	case 2: // Toggle
		power = power == HIGH ? LOW : HIGH;
		hold_count = 0; // disable hold on
		Serial.printf("PWR: Toggle state, relay %d\n", relay);
		break;
	case 3: // Hold On
		if (settings.hold_time) {
			// if hold on enabled
			if (!power) {
				relay_set(HIGH, relay); // set high power if is not
			}
			hold_count = settings.hold_time;
		}
		else {
			// no hold time, simple turn off
			power = LOW;
		}
		Serial.printf("PWR: Hold, relay %d\n", relay);
	}
	if (!hold_count) {
		relay_set(power, relay);
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
	mqtt_data_cb(stopic, (byte*)svalue, strlen(svalue));
}

void restart() {
	Serial.println("APP: Restarting");
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

// every second
void every_second() {
	// uptime
	seconds_counter++;
	if (seconds_counter >= 3600) {
		seconds_counter = 0;
		uptime++;
	}

	WIFI_Check(wificheckflag);
	wificheckflag = WIFI_RESTART;
	if (WiFi.status() == WL_CONNECTED) {
#ifdef USE_BLYNK
		if (blynk_enabled) {
			if (!Blynk.connected()) {
				if (!blynk_counter) {
					blynk_counter = BLYNK_RETRY_SECS;
					Serial.print("Trying to connect to blynk...");
					if (Blynk.connect()) {
						Serial.println("connected");
					}
					else {
						Serial.println("failed");
					}
				}
				else {
					blynk_counter--;
				}
			}
		}
#endif // USE_BLYNK

#ifdef USE_MQTT
		if (mqtt_enabled) {
			if (!mqttClient.connected()) {
				Serial.printf("MQT: disconnected with state %d\n", mqttClient.state());
				if (!mqtt_counter) {
					reconnect_mqtt();
				}
				else {
					mqtt_counter--;
				}
			}
		}
#endif // USE_MQTT
	}

	// check if on hold
	if (hold_count) {
		hold_count--;
		if (!hold_count) {
			// turn off relay
			do_cmnd_power(1, 0);
		}
	}

	if (settings.tele_period) {
		tele_period++;
		//if (tele_period == settings.tele_period - 1) {
		//	XsnsCall(FUNC_XSNS_PREP_BEFORE_TELEPERIOD);
		//}
		if (tele_period >= settings.tele_period) {
			tele_period = 0;

#ifdef USE_MQTT
			if (mqtt_enabled) {
				telemetry_mqtt();

				//mqtt_data[0] = '\0';
				//if (MqttShowSensor()) {
				//	MqttPublishPrefixTopic_P(2, PSTR(D_RSLT_SENSOR), Settings.flag.mqtt_sensor_retain);
				//}
			}
#endif // USE_MQTT
		}
	}
}

void mqtt_data_cb(char* topic, byte* data, unsigned int data_len) {

	char topicBuf[TOPSZ];
	char dataBuf[data_len + 1];
	char dataBufUc[128];
	char svalue[MESSZ];
	char stemp1[TOPSZ];
	char *p;
	char *type = NULL;
	uint16_t i = 0;
	uint16_t index;

	strncpy(topicBuf, topic, sizeof(topicBuf));
	memcpy(dataBuf, data, sizeof(dataBuf));
	dataBuf[sizeof(dataBuf) - 1] = 0;

	Serial.printf("MQT: Receive topic %s, data size %d, data %s\n", topicBuf, data_len, dataBuf);

	type = strrchr(topicBuf, '/') + 1;  // Last part of received topic is always the command (type)

	index = 1;
	if (type != NULL) {
		for (i = 0; i < strlen(type); i++) {
			type[i] = toupper(type[i]);
		}
		while (isdigit(type[i - 1])) {
			i--;
		}
		if (i < strlen(type)) {
			index = atoi(type + i);
		}
		type[i] = '\0';
	}

	for (i = 0; i <= sizeof(dataBufUc); i++) {
		dataBufUc[i] = toupper(dataBuf[i]);
	}

	Serial.printf("MQT: Index %d, type %s, data %s\n", index, type, dataBufUc);

	if (type != NULL) {
		snprintf_P(svalue, sizeof(svalue), PSTR("{\"Command\":\"Error\"}"));
		if (!strcmp(dataBufUc, "?")) {
			data_len = 0;
		}
		int16_t payload = -99;               // No payload
		uint16_t payload16 = 0;
		long lnum = strtol(dataBuf, &p, 10);
		if (p != dataBuf) {
			payload = (int16_t)lnum;          // -32766 - 32767
			payload16 = (uint16_t)lnum;       // 0 - 65535
		}

		if (!strcmp_P(dataBufUc, PSTR("OFF"))) {
			payload = 0;
		}
		if (!strcmp_P(dataBufUc, PSTR("ON"))) {
			payload = 1;
		}
		if (!strcmp_P(dataBufUc, PSTR("TOGGLE"))) {
			payload = 2;
		}
		if (!strcmp_P(dataBufUc, PSTR("HOLD"))) {
			payload = 3;
		}

		if (!strcmp_P(type, PSTR("POWER")) && (index > 0) && (index <= MaxRelay || !MaxRelay)) {
			if ((payload < 0) || (payload > 3)) {
				payload = 2;
			}
			do_cmnd_power(index, payload);
			return;
		}
		//else if (!strcmp_P(type, PSTR("STATUS"))) {
		//	if ((payload < 0) || (payload > MAX_STATUS)) {
		//		payload = 99;
		//	}
		//	publish_status(payload);
		//	fallbacktopic = 0;
		//	return;
		//}
		//else if (!strcmp_P(type, PSTR("PWM")) && (index > pwm_idxoffset) && (index <= 5)) {
		//	if ((payload >= 0) && (payload <= PWM_RANGE) && (pin[GPIO_PWM1 + index - 1] < 99)) {
		//		sysCfg.pwmvalue[index - 1] = payload;
		//		analogWrite(pin[GPIO_PWM1 + index - 1], payload);
		//	}
		//	snprintf_P(svalue, sizeof(svalue), PSTR("{\"PWM\":{"));
		//	bool first = true;
		//	for (byte i = 0; i < 5; i++) {
		//		if (pin[GPIO_PWM1 + i] < 99) {
		//			snprintf_P(svalue, sizeof(svalue), PSTR("%s%s\"PWM%d\":%d"), svalue, first ? "" : ", ", i + 1, sysCfg.pwmvalue[i]);
		//			first = false;
		//		}
		//	}
		//	snprintf_P(svalue, sizeof(svalue), PSTR("%s}}"), svalue);
		//}
		else if (!strcmp_P(type, PSTR("TELEPERIOD"))) {
			if ((payload >= 0) && (payload < 3601)) {
				settings.tele_period = (1 == payload) ? TELE_PERIOD : payload;
				if ((settings.tele_period > 0) && (settings.tele_period < 10)) {
					settings.tele_period = 10;   // Do not allow periods < 10 seconds
				}
				//tele_period = settings.tele_period;
			}
			snprintf_P(svalue, sizeof(svalue), PSTR("{\"TelePeriod\":\"%d Sec\"}"), settings.tele_period);
		}
		else if (!strcmp_P(type, PSTR("RESTART"))) {
			switch (payload) {
			case 1:
				restart_flag = 2;
				snprintf_P(svalue, sizeof(svalue), PSTR("{\"Restart\":\"Restarting\"}"));
				break;
			case 99:
				Serial.println("APP: Restarting");
				ESP.restart();
				break;
			default:
				snprintf_P(svalue, sizeof(svalue), PSTR("{\"Restart\":\"1 to restart\"}"));
			}
		}
		else if (!strcmp_P(type, PSTR("RESET"))) {
			switch (payload) {
			case 1:
				restart_flag = 211;
				snprintf_P(svalue, sizeof(svalue), PSTR("{\"Reset\":\"Reset and Restarting\"}"));
				break;
			case 2:
				restart_flag = 212;
				snprintf_P(svalue, sizeof(svalue), PSTR("{\"Reset\":\"Erase, Reset and Restarting\"}"));
				break;
			default:
				snprintf_P(svalue, sizeof(svalue), PSTR("{\"Reset\":\"1 to reset\"}"));
			}
		}
		else if (!strcmp_P(type, PSTR("WIFICONFIG"))) {
			if ((payload >= WIFI_RESTART) && (payload < MAX_WIFI_OPTION)) {
				settings.wifi_config = payload;
				wificheckflag = settings.wifi_config;
				snprintf_P(stemp1, sizeof(stemp1), wificfg[settings.wifi_config]);
				snprintf_P(svalue, sizeof(svalue), PSTR("{\"WifiConfig\":\"%s selected\"}"), stemp1);
				if (WIFI_State() != WIFI_RESTART) {
					snprintf_P(svalue, sizeof(svalue), PSTR("%s after restart"), svalue);
					restart_flag = 2;
				}
			}
			else {
				snprintf_P(stemp1, sizeof(stemp1), wificfg[settings.wifi_config]);
				snprintf_P(svalue, sizeof(svalue), PSTR("{\"WifiConfig\":\"%d (%s)\"}"), settings.wifi_config, stemp1);
			}
		}
		else if (!strcmp_P(type, PSTR("HOLDTIME"))) {
			if ((payload >= 0) && (payload < 3600)) {
				settings.hold_time = payload;
			}
			snprintf_P(svalue, sizeof(svalue), PSTR("{\"HoldTime\":%d}"), settings.hold_time);
		}
		//else if (!strcmp_P(type, PSTR("LEDPOWER"))) {
		//	if ((payload >= 0) && (payload <= 2)) {
		//		sysCfg.ledstate &= 8;
		//		switch (payload) {
		//		case 0: // Off
		//		case 1: // On
		//			sysCfg.ledstate = payload << 3;
		//			break;
		//		case 2: // Toggle
		//			sysCfg.ledstate ^= 8;
		//			break;
		//		}
		//		blinks = 0;
		//		setLed(sysCfg.ledstate & 8);
		//	}
		//	snprintf_P(svalue, sizeof(svalue), PSTR("{\"LedPower\":\"%s\"}"), getStateText(bitRead(sysCfg.ledstate, 3)));
		//}
		else if (!strcmp_P(type, PSTR("LEDMODE"))) {
			if ((payload >= 0) && (payload < MAX_LED_OPTION)) {
				settings.led_mode = payload;
				if (!settings.led_mode) {
					led_set(0);
				}
			}
			snprintf_P(svalue, sizeof(svalue), PSTR("{\"LedMode\":%d}"), settings.led_mode);
		}
		else {
			type = NULL;
		}
	}
#ifdef USE_MQTT
	if (type == NULL) {
		led_blinks++;
		snprintf_P(topicBuf, sizeof(topicBuf), PSTR("COMMAND"));
		snprintf_P(svalue, sizeof(svalue), PSTR("{\"Command\":\"Unknown\"}"));
		type = (char*)topicBuf;
	}
	if (svalue[0] != '\0') {
		char stopic[TOPSZ];
		sprintf(stopic, PUB_PREFIX "/%s/%s", settings.mqtt_topic, type);
		mqttClient.publish(stopic, svalue);
	}
#endif // USE_MQTT
}

#ifdef USE_BLYNK

void update_blynk(int relay) {
	if ((relay < 1) || (relay > MaxRelay)) {
		relay = 1;
	}
	int state = MaxRelay ? digitalRead(settings.relay_pin[relay - 1]) : power_state;
	Blynk.virtualWrite(relay, state);
	Serial.printf("BLK: Write on pin V%d value %d\n", relay, state);
}

void publish_blynk(int sensor) {
	//if (analog_flag) {
	//	int v = getAdc0();
	//	Blynk.virtualWrite(BLYNK_ANALOG_PIN, v);
	//	Serial.print("BLNK: V");
	//	Serial.print(BLYNK_ANALOG_PIN);
	//	Serial.print("=");
	//	Serial.println(v);
	//}
	if ((sensor < 1) || (sensor > MAX_SENSOR)) {
		sensor = 1;
	}
	WidgetLED* wLed = sensorLED[sensor - 1];
	if (wLed) {
		if (sensor_state[sensor - 1]) {
			wLed->on();
		}
		else {
			wLed->off();
		}
	}

	//Blynk.virtualWrite(BLYNK_ANALOG_PIN + sensor, sensorStates[sensor - 1]);
	Serial.printf("BLK: Write on pin V%d value %d\n", BLYNK_ANALOG_PIN + sensor, sensor_state[sensor - 1]);
}

// Every time we connect to the cloud...
BLYNK_CONNECTED() {
	// Request the latest state from the server
	//Blynk.syncVirtual(V2);

	// Alternatively, you could override server state using:
	update_blynk(1);
	for (byte i = 2; i <= MaxRelay; i++) {
		update_blynk(i);
	}
	for (byte i = 0; i < MAX_SENSOR; i++) {
		if (settings.sensor_mode[i] != NO_SENSOR) {
			publish_blynk(i + 1);
		}
	}
}

BLYNK_READ_DEFAULT() {
	int n = request.pin;
	if (n < BLYNK_ANALOG_PIN) {
		update_blynk(n);
	}
	else {
		n -= BLYNK_ANALOG_PIN;
		publish_blynk(n);
	}
}

BLYNK_WRITE_DEFAULT() {
	int relay = request.pin;
	int command = param.asInt();
	Serial.print("BLYNK: Pin ");
	Serial.print(relay);
	Serial.print(", Param ");
	Serial.println(param.asStr());

	if (relay != 1 && ((relay < 1) || (relay > MaxRelay))) {
		Serial.print("BLYNK: Invalid pin");
		Serial.println(relay);
	}

	switch (command) {
	case 0:
		do_cmnd_power(relay, 0);
		break;
	case 1:
		do_cmnd_power(relay, 1);
		break;
	case 2:
		do_cmnd_power(relay, 2);
		break;
	default:
		Serial.print("BLYNK: Command unknown");
		Serial.println(command);
		break;
	}
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

#endif // USE_BLYNK

#ifdef USE_MQTT

void reconnect_mqtt() {
	char topic[TOPSZ];

	mqtt_counter = MQTT_RETRY_SECS;

	sprintf(topic, PUB_PREFIX2 "/%s/LWT", settings.mqtt_topic);
	Serial.print("MQT: Attempting connection...\n");
	if (mqttClient.connect(mqttClientID, settings.mqtt_user, settings.mqtt_pwd, topic, 1, true, "Offline")) {
		// Once connected, publish an announcement...
		mqttClient.publish(topic, "Online", true);
		// ... and resubscribe
		sprintf(topic, SUB_PREFIX "/%s/+", settings.mqtt_topic);
		mqttClient.subscribe(topic);
		mqttClient.loop();  // Solve LmacRxBlk:1 messages
		Serial.printf("MQT: Connected, subscribe topic %s\n", topic);

		if (settings.tele_period) {
			tele_period = settings.tele_period - 3;
		}
	}
	else {
		Serial.print("MQT: Connect failed\n");
	}
}

void update_mqtt(int relay) {
	int state = MaxRelay ? digitalRead(settings.relay_pin[relay - 1]) : power_state;
	char topic[TOPSZ];
	char stateString[5];
	if (settings.module == 1) { // Sonoff Dual
		sprintf(topic, PUB_PREFIX "/%s/POWER%d", settings.mqtt_topic, relay);
	}
	else {
		sprintf(topic, PUB_PREFIX "/%s/POWER", settings.mqtt_topic);
	}
	strcpy(stateString, getStateText(state));
	mqttClient.publish(topic, stateString, false);
	Serial.printf("MQT: Publish on topic %s payload %s\n", topic, stateString);
}

void publish_mqtt(int sensor) {
	char topic[TOPSZ];
	char stateString[5];

	//if (analog_flag) {
	//	sprintf(topic, "stat/%s/SENSOR0", settings.mqttTopic);
	//	sprintf(stateString, "%d", getAdc0());
	//	mqttClient.publish(topic, stateString, true);
	//	Serial.print("MQTT: ");
	//	Serial.println(topic);
	//	Serial.println(stateString);
	//}
	if ((sensor < 1) || (sensor > MAX_SENSOR)) {
		sensor = 1;
	}
	sprintf(topic, PUB_PREFIX "/%s/SENSOR%d", settings.mqtt_topic, sensor);
	memcpy(stateString, getStateText(sensor_state[sensor - 1]), 5);
	mqttClient.publish(topic, stateString, false);
	Serial.printf("MQT: Publish on topic %s payload %s\n", topic, stateString);
}

void telemetry_mqtt()
{
	char stemp1[16];

	char topic[TOPSZ];
	char mqtt_data[MESSZ + TOPSZ];              // MQTT publish buffer (MESSZ) and web page ajax buffer (MESSZ + TOPSZ)

	//snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s{\"Time\":\"%s\",\"Uptime\":%d"), mqtt_data, GetDateAndTime().c_str(), uptime);
	snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("{\"Uptime\":%d,\"Ledmode\":%d,\"Holdtime\":%d\"Teleperiod\":%d"), uptime, settings.led_mode, settings.hold_time, settings.tele_period);
//#ifdef USE_ADC_VCC
//	dtostrfd((double)ESP.getVcc() / 1000, 3, stemp1);
//	snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"Vcc\":%s"), mqtt_data, stemp1);
//#endif
//	for (byte i = 0; i < devices_present; i++) {
//		snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"%s\":\"%s\""), mqtt_data, GetPowerDevice(stemp1, i + 1, sizeof(stemp1)), GetStateText(bitRead(power, i)));
//	}
	snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s,\"Wifi\":{\"AP\":%d,\"SSId\":\"%s\",\"RSSI\":%d,\"APMAC\":\"%s\"}}"),
		mqtt_data, settings.wifi_active + 1, settings.wifi_ssid[settings.wifi_active], WIFI_getRSSIasQuality(WiFi.RSSI()), WiFi.BSSIDstr().c_str());

	sprintf(topic, PUB_PREFIX2 "/%s/STATE", settings.mqtt_topic);
	mqttClient.publish(topic, mqtt_data, false);
	Serial.printf("MQT: Publish on topic %s payload %s\n", topic, mqtt_data);

	update_mqtt(1);
	for (byte i = 2; i <= MaxRelay; i++) {
		update_mqtt(i);
	}
	for (byte i = 1; i <= MAX_SENSOR; i++) {
		if (settings.sensor_mode[i - 1] != NO_SENSOR) {
			publish_mqtt(i);
		}
	}
}
#endif // USE_MQTT

