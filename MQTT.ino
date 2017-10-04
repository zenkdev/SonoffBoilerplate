#ifdef USE_MQTT

#include <PubSubClient.h>
#ifndef MESSZ
#define MESSZ                368          // Max number of characters in JSON message string (4 x DS18x20 sensors)
#endif
#if (MQTT_MAX_PACKET_SIZE -TOPSZ -7) < MESSZ  // If the max message size is too small, throw an error at compile time
// See pubsubclient.c line 359
#error "MQTT_MAX_PACKET_SIZE is too small in libraries/PubSubClient/src/PubSubClient.h, increase it to at least 467"
#endif

WiFiClient espClient;
PubSubClient mqttClient(espClient);

static bool MQTT_ENABLED = true;

unsigned long lastMQTTConnectionAttempt = 0;

#ifdef USE_DISCOVERY
/*********************************************************************************************\
* mDNS
\*********************************************************************************************/

#ifdef MQTT_HOST_DISCOVERY
boolean mdns_discoverMQTTServer()
{
	char ip_str[20];
	int n;

	if (!mDNSbegun) {
		return false;
	}

	n = MDNS.queryService("mqtt", "tcp");  // Search for mqtt service

	if (n > 0) {
		// Note: current strategy is to get the first MQTT service (even when many are found)
		IPtoCharArray(MDNS.IP(0), ip_str, 20);

		strlcpy(settings.mqttHost, ip_str, sizeof(settings.mqttHost));
		settings.mqttPort = MDNS.port(0);
	}

	return n > 0;
}
#endif  // MQTT_HOST_DISCOVERY

void IPtoCharArray(IPAddress address, char *ip_str, size_t size)
{
	String str = String(address[0]);
	str += ".";
	str += String(address[1]);
	str += ".";
	str += String(address[2]);
	str += ".";
	str += String(address[3]);
	str.toCharArray(ip_str, size);
}
#endif  // USE_DISCOVERY

void mqttfy(byte option, char* str)
{
	// option 0 = replace by underscore
	// option 1 = delete character  
	uint16_t i = 0;
	while (str[i] > 0) {
		//        if ((str[i] == '/') || (str[i] == '+') || (str[i] == '#') || (str[i] == ' ')) {
		if ((str[i] == '+') || (str[i] == '#') || (str[i] == ' ')) {
			if (option) {
				uint16_t j = i;
				while (str[j] > 0) {
					str[j] = str[j + 1];
					j++;
				}
				i--;
			}
			else {
				str[i] = '_';
			}
		}
		i++;
	}
}

void mqttDataCb(char* topic, byte* data, unsigned int data_len) {

	char topicBuf[TOPSZ];
	char dataBuf[data_len + 1];
	char dataBufUc[128];
	char svalue[MESSZ];
	char *p;
	char *type = NULL;
	uint16_t i = 0;
	uint16_t index;

	strncpy(topicBuf, topic, sizeof(topicBuf));
	memcpy(dataBuf, data, sizeof(dataBuf));
	dataBuf[sizeof(dataBuf) - 1] = 0;

	Serial.print("MQTT: Receive topic ");
	Serial.print(topicBuf);
	Serial.print(", data size ");
	Serial.print(data_len);
	Serial.print(", data ");
	Serial.println(dataBuf);

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

	Serial.print("MQTT: Index ");
	Serial.print(index);
	Serial.print(", Type ");
	Serial.print(type);
	Serial.print(", Data ");
	Serial.println(dataBufUc);

	if (type != NULL) {
		snprintf_P(svalue, sizeof(svalue), PSTR("{\"Command\":\"Error\"}"));
		//if (sysCfg.ledstate & 0x02) {
		//	blinks++;
		//}

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

		if (!strcmp_P(type, PSTR("POWER")) && (index > 0) && (index <= MaxRelay)) {
			if ((payload < 0) || (payload > 2)) {
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
		//else if (!strcmp_P(type, PSTR("TELEPERIOD"))) {
		//	if ((payload >= 0) && (payload < 3601)) {
		//		sysCfg.tele_period = (1 == payload) ? TELE_PERIOD : payload;
		//		if ((sysCfg.tele_period > 0) && (sysCfg.tele_period < 10)) {
		//			sysCfg.tele_period = 10;   // Do not allow periods < 10 seconds
		//		}
		//		tele_period = sysCfg.tele_period;
		//	}
		//	snprintf_P(svalue, sizeof(svalue), PSTR("{\"TelePeriod\":\"%d%s\"}"), sysCfg.tele_period, (sysCfg.flag.value_units) ? " Sec" : "");
		//}
		else if (!strcmp_P(type, PSTR("RESTART"))) {
			switch (payload) {
			case 1:
				restartflag = 2;
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
				restartflag = 211;
				snprintf_P(svalue, sizeof(svalue), PSTR("{\"Reset\":\"Reset and Restarting\"}"));
				break;
			case 2:
				restartflag = 212;
				snprintf_P(svalue, sizeof(svalue), PSTR("{\"Reset\":\"Erase, Reset and Restarting\"}"));
				break;
			default:
				snprintf_P(svalue, sizeof(svalue), PSTR("{\"Reset\":\"1 to reset\"}"));
			}
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
		//else if (!strcmp_P(type, PSTR("LEDSTATE"))) {
		//	if ((payload >= 0) && (payload < MAX_LED_OPTION)) {
		//		sysCfg.ledstate = payload;
		//		if (!sysCfg.ledstate) {
		//			setLed(0);
		//		}
		//	}
		//	snprintf_P(svalue, sizeof(svalue), PSTR("{\"LedState\":%d}"), sysCfg.ledstate);
		//}
		else {
			type = NULL;
		}
	}
	if (type == NULL) {
		//blinks = 201;
		snprintf_P(topicBuf, sizeof(topicBuf), PSTR("COMMAND"));
		snprintf_P(svalue, sizeof(svalue), PSTR("{\"Command\":\"Unknown\"}"));
		type = (char*)topicBuf;
	}
	if (svalue[0] != '\0') {
		char stopic[TOPSZ];
		sprintf(stopic, "stat/%s/%s", settings.mqttTopic, type);
		mqttClient.publish(stopic, svalue);
	}
}

void MQTT_setup() {
	if (strlen(settings.mqttHost) == 0) {
		MQTT_ENABLED = false;
	}
	if (MQTT_ENABLED) {
#ifdef USE_DISCOVERY
#ifdef MQTT_HOST_DISCOVERY
		mdns_discoverMQTTServer();
#endif  // MQTT_HOST_DISCOVERY
#endif  // USE_DISCOVERY
		mqttClient.setServer(settings.mqttHost, settings.mqttPort);
		mqttClient.setCallback(mqttDataCb);
	}
}

void MQTT_publish(int sensor) {
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
	sprintf(topic, "stat/%s/SENSOR%d", settings.mqttTopic, sensor);
	memcpy(stateString, getStateText(sensorStates[sensor - 1]), 5);
	mqttClient.publish(topic, stateString, true);
	Serial.print("MQTT: ");
	Serial.println(topic);
	Serial.println(stateString);
}

void MQTT_loop() {
	if (MQTT_ENABLED) {
		if (!mqttClient.connected()) {
			if (lastMQTTConnectionAttempt == 0 || millis() > lastMQTTConnectionAttempt + 3 * 60 * 1000) {
				lastMQTTConnectionAttempt = millis();
				Serial.print("Trying to connect to mqtt...");
				if (mqttClient.connect(settings.mqttClientID)) {
					//// Once connected, publish an announcement...
					//mqttClient.publish(settings.mqttTopic, "hello world");
					char topic[TOPSZ];
					Serial.println("connected");
					sprintf(topic, "cmnd/%s/+", settings.mqttTopic);
					mqttClient.subscribe(topic);
					Serial.println(topic);

					MQTT_update(1);
					for (byte idx = 2; idx <= MaxRelay; idx++) {
						MQTT_update(idx);
					}
					MQTT_publish(true);
				}
				else {
					Serial.println("failed");
				}
			}
		}
		else {
			mqttClient.loop();
		}
	}
}

void MQTT_update(int relay) {
	int state = MaxRelay ? digitalRead(settings.relayPins[relay - 1]) : powerState;
	char topic[TOPSZ];
	char stateString[5];
	if (settings.module == 1) { // Sonoff Dual
		sprintf(topic, "stat/%s/POWER%d", settings.mqttTopic, relay);
	}
	else {
		sprintf(topic, "stat/%s/POWER", settings.mqttTopic);
	}
	strcpy(stateString, getStateText(state));
	mqttClient.publish(topic, stateString, true);
	Serial.print("MQTT: ");
	Serial.println(topic);
	Serial.println(stateString);
}

#endif

