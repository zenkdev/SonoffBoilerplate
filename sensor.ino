#define MOTION_DETECTED  "Motion detected"
#define NO_MOTION        "No motion"
#define HIGH_SOUND       "High sound"
#define LOW_SOUND        "Low sound"

#define SENSOR_DEBOUNCE  800 // Sensor debounce time millisconds

// Sensor names for enum
// enum sensor_t { NO_SENSOR, MOTION_SENSOR, SOUND_SENSOR, SENSOR_END };
const char sensors[3][18] PROGMEM = { "None", "HC-SR501 (motion)", "KY-037 (sound)" };
unsigned long pTimeLast[MAX_SENSOR]; // Last counter time in milli seconds
int pwrsns = LOW; // prev power state
int oldStates[MAX_SENSOR] = { LOW, LOW, LOW, LOW };


uint16_t getAdc0()
{
	uint16_t alr = 0;
	for (byte i = 0; i < 32; i++) {
		alr += analogRead(A0);
		delay(1);
	}
	return alr >> 5;
}

void sensor_update(byte index)
{
	pTimeLast[index - 1] = millis();
	sensorStates[index - 1] = HIGH;
}

void sensor_update1()
{
	sensor_update(1);
}

void sensor_update2()
{
	sensor_update(2);
}

void sensor_update3()
{
	sensor_update(3);
}

void sensor_update4()
{
	sensor_update(4);
}

void sensor_setup() {
	typedef void(*function) ();
	function sensor_callbacks[] = { sensor_update1, sensor_update2, sensor_update3, sensor_update4 };

	for (byte i = 0; i < MAX_SENSOR; i++) {
		if (settings.sensorModes[i] != NO_SENSOR) {
			pinMode(settings.sensorPins[i], INPUT);
			attachInterrupt(settings.sensorPins[i], sensor_callbacks[i], RISING);
		}
		if (settings.sensorModes[i] == SOUND_SENSOR) {
			analog_flag = true;
			pinMode(A0, INPUT);
		}
	}
}

// every 0.1 second
void sensor_state() {
	int npwr = 0;
	for (byte i = 0; i < MAX_SENSOR; i++) {
		if (settings.sensorModes[i] != NO_SENSOR) {
			unsigned long pTime = millis() - pTimeLast[i];
			if (pTime > SENSOR_DEBOUNCE && sensorStates[i] == HIGH) {
				pTimeLast[i] = millis();
				sensorStates[i] = digitalRead(settings.sensorPins[i]);
			}
			npwr |= sensorStates[i];
		}
	}

	if (npwr != pwrsns) {
		pwrsns = npwr;
		do_cmnd_power(1, !pwrsns ? 3 : pwrsns); // hold on instead of turn off
	}
}

// every 0.2 second
void sensor_publish() {
	for (byte i = 0; i < MAX_SENSOR; i++) {
		if (settings.sensorModes[i] != NO_SENSOR) {
			if (oldStates[i] != sensorStates[i]) {
				oldStates[i] = sensorStates[i];
#ifdef USE_BLYNK
				BLYNK_publish(i + 1);
#endif

#ifdef USE_MQTT
				MQTT_publish(i + 1);
#endif
			}
		}
	}
}

char* sensor_getStateText(byte type, int state)
{
	if (type == MOTION_SENSOR) {
		if (state) {
			return MOTION_DETECTED;
		}
		return NO_MOTION;
	}
	else if (type == SOUND_SENSOR) {
		if (state) {
			return HIGH_SOUND;
		}
		return LOW_SOUND;
	}
	else {
		if (state) {
			return MQTT_STATUS_ON;
		}
		return MQTT_STATUS_OFF;
	}
}

//void counter_mqttPresent(uint8_t* djson)
//{
//	char stemp[16];
//
//	byte dsxflg = 0;
//	for (byte i = 0; i < MAX_COUNTERS; i++) {
//		if (pin[GPIO_CNTR1 + i] < 99) {
//			if (bitRead(sysCfg.pCounterType, i)) {
//				dtostrfd((double)rtcMem.pCounter[i] / 1000, 3, stemp);
//			}
//			else {
//				dsxflg++;
//				dtostrfd(rtcMem.pCounter[i], 0, stemp);
//			}
//			snprintf_P(mqtt_data, sizeof(mqtt_data), PSTR("%s, \"" D_COUNTER "%d\":%s"), mqtt_data, i + 1, stemp);
//			*djson = 1;
//#ifdef USE_DOMOTICZ
//			if (1 == dsxflg) {
//				domoticz_sensor6(rtcMem.pCounter[i]);
//				dsxflg++;
//			}
//#endif  // USE_DOMOTICZ
//		}
//	}
//}

#ifdef USE_WEBSERVER

String sensor_webPresent()
{
	String page = "";
	char sensor[128];

	for (byte i = 0; i < MAX_SENSOR; i++) {
		if (settings.sensorModes[i] > NO_SENSOR) {
			snprintf_P(sensor, sizeof(sensor), PSTR("<tr><th>GPIO%d</th><td>%s</td></tr>"), settings.sensorPins[i], sensor_getStateText(settings.sensorModes[i], sensorStates[i]));
			page += sensor;
		}
	}
	return page;
}

#endif  // USE_WEBSERVER

