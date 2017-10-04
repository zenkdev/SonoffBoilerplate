#ifdef USE_BLYNK
/**********
   VPIN % 5
   0 off
   1 on
   2 toggle
   3 value
   4 led
 ***********/

#include <BlynkSimpleEsp8266.h>

#define BLYNK_ANALOG_PIN 10        // [Blynk] Analog virtual pin
#define BLYNK_PRINT      Serial    // Comment this out to disable prints and save space

static bool BLYNK_ENABLED = true;

unsigned long lastBLYNKConnectionAttempt = 0;

WidgetLED* sensorled[MAX_SENSOR];

void BLYNK_setup() {
	if (strlen(settings.blynkToken) == 0) {
		BLYNK_ENABLED = false;
	}
	if (BLYNK_ENABLED) {
		Blynk.config(settings.blynkToken, settings.blynkServer, settings.blynkPort);
		for (byte i = 0; i < MAX_SENSOR; i++) {
			if (settings.sensorModes[i] != NO_SENSOR) {
				sensorled[i] = new WidgetLED(BLYNK_ANALOG_PIN + 1 + i);
			}
		}
	}
}

void BLYNK_publish(int sensor) {
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
	WidgetLED* wLed = sensorled[sensor - 1];
	if (wLed) {
		if (sensorStates[sensor - 1]) {
			wLed->on();
		}
		else {
			wLed->off();
		}
	}

	//Blynk.virtualWrite(BLYNK_ANALOG_PIN + sensor, sensorStates[sensor - 1]);
	Serial.print("BLNK: V");
	Serial.print(BLYNK_ANALOG_PIN + sensor);
	Serial.print("=");
	Serial.println(sensorStates[sensor - 1]);
}

void BLYNK_loop() {
	//blynk connect and run loop
	if (BLYNK_ENABLED) {
		if (!Blynk.connected()) {
			if (lastBLYNKConnectionAttempt == 0 || millis() > lastBLYNKConnectionAttempt + 3 * 60 * 1000) {
				lastBLYNKConnectionAttempt = millis();
				Serial.print("Trying to connect to blynk...");
				if (Blynk.connect()) {
					Serial.println("connected");
				}
				else {
					Serial.println("failed");
				}
			}
		}
		else {
			Blynk.run();
		}
	}
}

void BLYNK_update(int relay) {
	if ((relay < 1) || (relay > MaxRelay)) {
		relay = 1;
	}
	int state = MaxRelay ? digitalRead(settings.relayPins[relay - 1]) : powerState;
	Blynk.virtualWrite(relay, state);
	Serial.print("BLNK: V");
	Serial.print(relay);
	Serial.print("=");
	Serial.println(state);
}

// Every time we connect to the cloud...
BLYNK_CONNECTED() {
	// Request the latest state from the server
	//Blynk.syncVirtual(V2);

	// Alternatively, you could override server state using:
	BLYNK_update(1);
	for (byte idx = 2; idx <= MaxRelay; idx++) {
		BLYNK_update(idx);
	}
	for (byte i = 0; i < MAX_SENSOR; i++) {
		if (settings.sensorModes[i] != NO_SENSOR) {
			BLYNK_publish(i + 1);
		}
	}
}


BLYNK_READ_DEFAULT() {
	int n = request.pin;
	if (n < BLYNK_ANALOG_PIN) {
		BLYNK_update(n);
	}
	else {
		n -= BLYNK_ANALOG_PIN;
		BLYNK_publish(n);
	}
}

BLYNK_WRITE_DEFAULT() {
	int relay = request.pin;
	int command = param.asInt();
	Serial.print("BLYNK: Pin ");
	Serial.print(relay);
	Serial.print(", Param ");
	Serial.println(param.asStr());

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

#endif

