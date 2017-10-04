#define MAX_GPIO_PIN       18   // Number of supported GPIO
#define MAX_MODULE         5

// Supported hardware modules
const char modules[MAX_MODULE][15] PROGMEM = { "Sonoff Basic", "Sonoff Dual", "Sonoff TH", "S20 Socket", "WeMos D1 mini" };

void module_setup() {
	switch (settings.module)
	{
	case 0: // Sonoff Basic
		settings.buttonPin = 0;
		settings.ledPin = 13;
		settings.ledInverted = true;
		settings.relayPins[0] = 12;
		settings.relayPins[1] = NOT_A_PIN;
		settings.relayPins[2] = NOT_A_PIN;
		settings.relayPins[3] = NOT_A_PIN;
		settings.sensorPins[0] = 14;
		settings.sensorPins[1] = NOT_A_PIN;
		settings.sensorPins[2] = NOT_A_PIN;
		settings.sensorPins[3] = NOT_A_PIN;
		settings.sensorModes[0] = NO_SENSOR;
		settings.sensorModes[1] = NO_SENSOR;
		settings.sensorModes[2] = NO_SENSOR;
		settings.sensorModes[3] = NO_SENSOR;
		break;
	case 1: // Sonoff Dual
		settings.buttonPin = 4;
		settings.ledPin = 13;
		settings.ledInverted = true;
		settings.relayPins[0] = 1;
		settings.relayPins[1] = 3;
		settings.relayPins[2] = NOT_A_PIN;
		settings.relayPins[3] = NOT_A_PIN;
		settings.sensorPins[0] = NOT_A_PIN;
		settings.sensorPins[1] = NOT_A_PIN;
		settings.sensorPins[2] = NOT_A_PIN;
		settings.sensorPins[3] = NOT_A_PIN;
		settings.sensorModes[0] = NO_SENSOR;
		settings.sensorModes[1] = NO_SENSOR;
		settings.sensorModes[2] = NO_SENSOR;
		settings.sensorModes[3] = NO_SENSOR;
		break;
	case 2: // Sonoff TH
		settings.buttonPin = 0;
		settings.ledPin = 13;
		settings.ledInverted = true;
		settings.relayPins[0] = 12;
		settings.relayPins[1] = NOT_A_PIN;
		settings.relayPins[2] = NOT_A_PIN;
		settings.relayPins[3] = NOT_A_PIN;
		settings.sensorPins[0] = 14;
		settings.sensorPins[1] = NOT_A_PIN;
		settings.sensorPins[2] = NOT_A_PIN;
		settings.sensorPins[3] = NOT_A_PIN;
		settings.sensorModes[0] = NO_SENSOR;
		settings.sensorModes[1] = NO_SENSOR;
		settings.sensorModes[2] = NO_SENSOR;
		settings.sensorModes[3] = NO_SENSOR;
		break;
	case 3: // S20 Socket
		settings.buttonPin = 0;
		settings.ledPin = 13;
		settings.ledInverted = true;
		settings.relayPins[0] = 12;
		settings.relayPins[1] = NOT_A_PIN;
		settings.relayPins[2] = NOT_A_PIN;
		settings.relayPins[3] = NOT_A_PIN;
		settings.sensorPins[0] = NOT_A_PIN;
		settings.sensorPins[1] = NOT_A_PIN;
		settings.sensorPins[2] = NOT_A_PIN;
		settings.sensorPins[3] = NOT_A_PIN;
		settings.sensorModes[0] = NO_SENSOR;
		settings.sensorModes[1] = NO_SENSOR;
		settings.sensorModes[2] = NO_SENSOR;
		settings.sensorModes[3] = NO_SENSOR;
		break;
	case 4: // WeMos D1 mini
		settings.buttonPin = 0;   // D3
		settings.ledPin = 2;      // D4
		settings.ledInverted = true;
		settings.relayPins[0] = NOT_A_PIN;
		settings.relayPins[1] = NOT_A_PIN;
		settings.relayPins[2] = NOT_A_PIN;
		settings.relayPins[3] = NOT_A_PIN;
		settings.sensorPins[0] = 14; // noise D5
		settings.sensorPins[1] = 12; // motion D6
		settings.sensorPins[2] = 13; // D7
		settings.sensorPins[3] = 15; // D8
		settings.sensorModes[0] = NO_SENSOR;
		settings.sensorModes[1] = NO_SENSOR;
		settings.sensorModes[2] = NO_SENSOR;
		settings.sensorModes[3] = NO_SENSOR;
		break;
	default:
		break;
	}
}
