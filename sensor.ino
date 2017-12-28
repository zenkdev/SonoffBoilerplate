#define MOTION_DETECTED  "Motion detected"
#define NO_MOTION        "No motion"
#define HIGH_SOUND       "High sound"
#define LOW_SOUND        "Low sound"

#define SENSOR_DEBOUNCE  800 // Sensor debounce time millisconds

// Sensor names for enum
// enum sensor_t { NO_SENSOR, MOTION_SENSOR, SOUND_SENSOR, SENSOR_END };
const char sensors[3][18] PROGMEM = { "None", "HC-SR501 (motion)", "KY-037 (sound)" };
ulong pTimeLast[MAX_SENSOR]; // Last counter time in milli seconds
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
  sensor_state[index - 1] = HIGH;
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

// every 0.1 second
void sensor_checkState() {
  int npwr = LOW;
  for (byte i = 0; i < MAX_SENSOR; i++) {
    if (settings.sensor_mode[i] != NO_SENSOR) {
      if (oldStates[i] != sensor_state[i]) {
        // publish new sensor state
        oldStates[i] = sensor_state[i];
#ifdef USE_BLYNK
		if (blynk_enabled) {
			publish_blynk(i + 1);
		}
#endif

#ifdef USE_MQTT
		if (mqtt_enabled) {
			publish_mqtt(i + 1);
		}
#endif
      }
      // calc total power sta
      npwr = npwr || sensor_state[i];

      unsigned long pTime = millis() - pTimeLast[i];
      if (pTime > SENSOR_DEBOUNCE && sensor_state[i] == HIGH) {
        // refresh sensor state, if it's time
        pTimeLast[i] = millis();
		sensor_state[i] = digitalRead(settings.sensor_pin[i]);
      }
    }
  }

  // got changed power state
  if (npwr != pwrsns) {
    // publish power
    pwrsns = npwr;
    do_cmnd_power(1, pwrsns ? 1 : 3); // hold on instead of turn off

	// led control
	if (settings.led_mode == LED_SYSTEM_SENSOR) {
		if (pwrsns) {
			led_blinks = 5; // Five blinks
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
    if (settings.sensor_mode[i] > NO_SENSOR) {
      snprintf_P(sensor, sizeof(sensor), PSTR("<tr><th>GPIO%d</th><td>%s</td></tr>"), settings.sensor_pin[i], sensor_getStateText(settings.sensor_mode[i], sensor_state[i]));
      page += sensor;
    }
  }
  return page;
}

#endif  // USE_WEBSERVER
