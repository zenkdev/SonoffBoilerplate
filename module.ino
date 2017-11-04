#define MAX_GPIO_PIN       18   // Number of supported GPIO
#define MAX_MODULE         5

// Supported hardware modules
const char modules[MAX_MODULE][15] PROGMEM = { "WeMos D1 mini", "Sonoff Basic", "Sonoff Dual", "Sonoff TH", "S20 Socket" };

void setup_module() {
	switch (settings.module)
	{
	case 0: // WeMos D1 mini
		settings.button_pin = 0;   // D3
		settings.led_pin = 2;      // D4
		settings.led_inverted = true;
		settings.relay_pin[0] = NOT_A_PIN;
		settings.relay_pin[1] = NOT_A_PIN;
		settings.relay_pin[2] = NOT_A_PIN;
		settings.relay_pin[3] = NOT_A_PIN;
		settings.sensor_pin[0] = 14; // noise D5
		settings.sensor_pin[1] = 12; // motion D6
		settings.sensor_pin[2] = 13; // D7
		settings.sensor_pin[3] = 15; // D8
		settings.sensor_mode[0] = NO_SENSOR;
		settings.sensor_mode[1] = NO_SENSOR;
		settings.sensor_mode[2] = NO_SENSOR;
		settings.sensor_mode[3] = NO_SENSOR;
		break;
	case 1: // Sonoff Basic
		settings.button_pin = 0;
		settings.led_pin = 13;
		settings.led_inverted = true;
		settings.relay_pin[0] = 12;
		settings.relay_pin[1] = NOT_A_PIN;
		settings.relay_pin[2] = NOT_A_PIN;
		settings.relay_pin[3] = NOT_A_PIN;
		settings.sensor_pin[0] = 14;
		settings.sensor_pin[1] = NOT_A_PIN;
		settings.sensor_pin[2] = NOT_A_PIN;
		settings.sensor_pin[3] = NOT_A_PIN;
		settings.sensor_mode[0] = NO_SENSOR;
		settings.sensor_mode[1] = NO_SENSOR;
		settings.sensor_mode[2] = NO_SENSOR;
		settings.sensor_mode[3] = NO_SENSOR;
		break;
	case 2: // Sonoff Dual
		settings.button_pin = 4;
		settings.led_pin = 13;
		settings.led_inverted = true;
		settings.relay_pin[0] = 1;
		settings.relay_pin[1] = 3;
		settings.relay_pin[2] = NOT_A_PIN;
		settings.relay_pin[3] = NOT_A_PIN;
		settings.sensor_pin[0] = NOT_A_PIN;
		settings.sensor_pin[1] = NOT_A_PIN;
		settings.sensor_pin[2] = NOT_A_PIN;
		settings.sensor_pin[3] = NOT_A_PIN;
		settings.sensor_mode[0] = NO_SENSOR;
		settings.sensor_mode[1] = NO_SENSOR;
		settings.sensor_mode[2] = NO_SENSOR;
		settings.sensor_mode[3] = NO_SENSOR;
		break;
	case 3: // Sonoff TH
		settings.button_pin = 0;
		settings.led_pin = 13;
		settings.led_inverted = true;
		settings.relay_pin[0] = 12;
		settings.relay_pin[1] = NOT_A_PIN;
		settings.relay_pin[2] = NOT_A_PIN;
		settings.relay_pin[3] = NOT_A_PIN;
		settings.sensor_pin[0] = 14;
		settings.sensor_pin[1] = NOT_A_PIN;
		settings.sensor_pin[2] = NOT_A_PIN;
		settings.sensor_pin[3] = NOT_A_PIN;
		settings.sensor_mode[0] = NO_SENSOR;
		settings.sensor_mode[1] = NO_SENSOR;
		settings.sensor_mode[2] = NO_SENSOR;
		settings.sensor_mode[3] = NO_SENSOR;
		break;
	case 4: // S20 Socket
		settings.button_pin = 0;
		settings.led_pin = 13;
		settings.led_inverted = true;
		settings.relay_pin[0] = 12;
		settings.relay_pin[1] = NOT_A_PIN;
		settings.relay_pin[2] = NOT_A_PIN;
		settings.relay_pin[3] = NOT_A_PIN;
		settings.sensor_pin[0] = NOT_A_PIN;
		settings.sensor_pin[1] = NOT_A_PIN;
		settings.sensor_pin[2] = NOT_A_PIN;
		settings.sensor_pin[3] = NOT_A_PIN;
		settings.sensor_mode[0] = NO_SENSOR;
		settings.sensor_mode[1] = NO_SENSOR;
		settings.sensor_mode[2] = NO_SENSOR;
		settings.sensor_mode[3] = NO_SENSOR;
		break;
	default:
		break;
	}
}

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

char* getStateText(int state)
{
	if (state) {
		return MQTT_STATUS_ON;
	}
	return MQTT_STATUS_OFF;
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
		if (settings.relay_pin[idx] != NOT_A_PIN) {
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

#ifdef USE_MQTT
#ifdef USE_DISCOVERY
/*********************************************************************************************/
//  mDNS
/*********************************************************************************************/

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

		strlcpy(settings.mqtt_host, ip_str, sizeof(settings.mqtt_host));
		settings.mqtt_port = MDNS.port(0);
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
#endif // USE_DISCOVERY

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

#endif // USE_MQTT

/*********************************************************************************************\
* Wifi
\*********************************************************************************************/

#define WIFI_CONFIG_SEC   180  // seconds before restart
#define WIFI_MANAGER_SEC  180  // seconds before restart
#define WIFI_CHECK_SEC    20   // seconds
#define WIFI_RETRY_SEC    30   // seconds

uint8_t _wificounter;
uint8_t _wifiretry;
uint8_t _wifistatus;
uint8_t _wpsresult;
uint8_t _wificonfigflag = 0;
uint8_t _wifiConfigCounter = 0;

int WIFI_getRSSIasQuality(int RSSI)
{
	int quality = 0;

	if (RSSI <= -100) {
		quality = 0;
	}
	else if (RSSI >= -50) {
		quality = 100;
	}
	else {
		quality = 2 * (RSSI + 100);
	}
	return quality;
}

boolean WIFI_configCounter()
{
	if (_wifiConfigCounter) {
		_wifiConfigCounter = WIFI_MANAGER_SEC;
	}
	return (_wifiConfigCounter);
}

extern "C" {
#include "user_interface.h"
}

void WIFI_wps_status_cb(wps_cb_status status);

void WIFI_wps_status_cb(wps_cb_status status)
{
	/* from user_interface.h:
	enum wps_cb_status {
	WPS_CB_ST_SUCCESS = 0,
	WPS_CB_ST_FAILED,
	WPS_CB_ST_TIMEOUT,
	WPS_CB_ST_WEP,      // WPS failed because that WEP is not supported
	WPS_CB_ST_SCAN_ERR, // can not find the target WPS AP
	};
	*/
	_wpsresult = status;
	if (WPS_CB_ST_SUCCESS == _wpsresult) {
		wifi_wps_disable();
	}
	else {
		Serial.printf("WIF: WPSconfig FAILED with status %d\n", _wpsresult);
		_wifiConfigCounter = 2;
	}
}

boolean WIFI_WPSConfigDone(void)
{
	return (!_wpsresult);
}

boolean WIFI_beginWPSConfig(void)
{
	_wpsresult = 99;
	if (!wifi_wps_disable()) {
		return false;
	}
	if (!wifi_wps_enable(WPS_TYPE_PBC)) {
		return false;  // so far only WPS_TYPE_PBC is supported (SDK 2.0.0)
	}
	if (!wifi_set_wps_cb((wps_st_cb_t)&WIFI_wps_status_cb)) {
		return false;
	}
	if (!wifi_wps_start()) {
		return false;
	}
	return true;
}

void WIFI_config(uint8_t type)
{
	if (!_wificonfigflag) {
		if (type >= WIFI_RETRY) {  // WIFI_RETRY and WIFI_WAIT
			return;
		}
#ifdef USE_EMULATION
		UDP_Disconnect();
#endif  // USE_EMULATION
		WiFi.disconnect();        // Solve possible Wifi hangs
		_wificonfigflag = type;
		_wifiConfigCounter = WIFI_CONFIG_SEC;   // Allow up to WIFI_CONFIG_SECS seconds for phone to provide ssid/pswd
		_wificounter = _wifiConfigCounter + 5;

		// start ticker with 0.5 because we start in AP mode and try to connect
		ticker.attach(0.5, tick);

		if (WIFI_RESTART == _wificonfigflag) {
			restartflag = 2;
		}
		else if (WIFI_SMARTCONFIG == _wificonfigflag) {
			Serial.print("WIF: SmartConfig active for 1 minute\n");
			WiFi.beginSmartConfig();
		}
		else if (WIFI_WPSCONFIG == _wificonfigflag) {
			if (WIFI_beginWPSConfig()) {
				Serial.print("WIF: WPSConfig active for 1 minute\n");
			}
			else {
				Serial.print("WIF: WPSConfig failed to start\n");
				_wifiConfigCounter = 3;
			}
		}
#ifdef USE_WEBSERVER
		else if (WIFI_MANAGER == _wificonfigflag) {
			Serial.print("WIF: WifiManager active for 1 minute\n");
			beginWifiManager();
		}
#endif  // USE_WEBSERVER
	}
}

void WIFI_begin(uint8_t flag)
{
	const char PhyMode[] = " BGN";

#ifdef USE_EMULATION
	UDP_Disconnect();
#endif  // USE_EMULATION
	if (!strncmp_P(ESP.getSdkVersion(), PSTR("1.5.3"), 5)) {
		Serial.print("WIF: Sonoff-Tasmota Patch issue 2186\n");
		WiFi.mode(WIFI_OFF);    // See https://github.com/esp8266/Arduino/issues/2186
	}
	WiFi.disconnect();
	WiFi.mode(WIFI_STA);      // Disable AP mode
	//if (sysCfg.sleep) {
	//	WiFi.setSleepMode(WIFI_LIGHT_SLEEP);  // Allow light sleep during idle times
	//}
	//  if (WiFi.getPhyMode() != WIFI_PHY_MODE_11N) {
	//    WiFi.setPhyMode(WIFI_PHY_MODE_11N);
	//  }
	if (!WiFi.getAutoConnect()) {
		WiFi.setAutoConnect(true);
	}
	//  WiFi.setAutoReconnect(true);
	switch (flag) {
	case 0:  // AP1
	case 1:  // AP2
		settings.wifi_active = flag;
		break;
	case 2:  // Toggle
		settings.wifi_active ^= 1;
	}        // 3: Current AP
	if (0 == strlen(settings.wifi_ssid[1])) {
		settings.wifi_active = 0;
	}
	//if (sysCfg.ip_address[0]) {
	//	WiFi.config(sysCfg.ip_address[0], sysCfg.ip_address[1], sysCfg.ip_address[2], sysCfg.ip_address[3]);  // Set static IP
	//}
	WiFi.hostname(Hostname);
	WiFi.begin(settings.wifi_ssid[settings.wifi_active], settings.wifi_pwd[settings.wifi_active]);
	Serial.printf("WIF: Connecting to AP%d %s in mode 11%c as %s...\n",
		settings.wifi_active + 1, settings.wifi_ssid[settings.wifi_active], PhyMode[WiFi.getPhyMode() & 0x3], Hostname);
}

void WIFI_check_ip()
{
	if ((WL_CONNECTED == WiFi.status()) && (static_cast<uint32_t>(WiFi.localIP()) != 0)) {
		_wificounter = WIFI_CHECK_SEC;
		_wifiretry = WIFI_RETRY_SEC;
		//addLog_P((_wifistatus != WL_CONNECTED) ? LOG_LEVEL_INFO : LOG_LEVEL_DEBUG_MORE, S_LOG_WIFI, PSTR(D_CONNECTED));
		Serial.print("WIF: Connected\n");
		if (_wifistatus != WL_CONNECTED) {
			//      addLog_P(LOG_LEVEL_INFO, PSTR("Wifi: Set IP addresses"));
			//sysCfg.ip_address[1] = (uint32_t)WiFi.gatewayIP();
			//sysCfg.ip_address[2] = (uint32_t)WiFi.subnetMask();
			//sysCfg.ip_address[3] = (uint32_t)WiFi.dnsIP();
		}
		_wifistatus = WL_CONNECTED;
	}
	else {
		_wifistatus = WiFi.status();
		switch (_wifistatus) {
		case WL_CONNECTED:
			Serial.print("WIF: Connect failed as no IP address received\n");
			_wifistatus = 0;
			_wifiretry = WIFI_RETRY_SEC;
			break;
		case WL_NO_SSID_AVAIL:
			Serial.print("WIF: Connect failed as AP cannot be reached\n");
			if (WIFI_WAIT == settings.wifi_config) {
				_wifiretry = WIFI_RETRY_SEC;
			}
			else {
				if (_wifiretry > (WIFI_RETRY_SEC / 2)) {
					_wifiretry = WIFI_RETRY_SEC / 2;
				}
				else if (_wifiretry) {
					_wifiretry = 0;
				}
			}
			break;
		case WL_CONNECT_FAILED:
			Serial.print("WIF: Connect failed with AP incorrect password\n");
			if (_wifiretry > (WIFI_RETRY_SEC / 2)) {
				_wifiretry = WIFI_RETRY_SEC / 2;
			}
			else if (_wifiretry) {
				_wifiretry = 0;
			}
			break;
		default:  // WL_IDLE_STATUS and WL_DISCONNECTED
			if (!_wifiretry || ((WIFI_RETRY_SEC / 2) == _wifiretry)) {
				Serial.print("WIF: Connect failed with AP timeout\n");
			}
			else {
				Serial.print("WIF: Attempting connection...\n");
			}
		}
		if (_wifiretry) {
			if (WIFI_RETRY_SEC == _wifiretry) {
				WIFI_begin(3);  // Select default SSID
			}
			if ((settings.wifi_config != WIFI_WAIT) && ((WIFI_RETRY_SEC / 2) == _wifiretry)) {
				WIFI_begin(2);  // Select alternate SSID
			}
			_wificounter = 1;
			_wifiretry--;
		}
		else {
			WIFI_config(WIFI_CONFIG_TOOL);
			_wificounter = 1;
			_wifiretry = WIFI_RETRY_SEC;
		}
	}
}

void WIFI_Check(uint8_t param)
{
	_wificounter--;
	switch (param) {
	case WIFI_SMARTCONFIG:
	case WIFI_MANAGER:
	case WIFI_WPSCONFIG:
		WIFI_config(param);
		break;
	default:
		if (_wifiConfigCounter) {
			_wifiConfigCounter--;
			_wificounter = _wifiConfigCounter + 5;
			if (_wifiConfigCounter) {
				if ((WIFI_SMARTCONFIG == _wificonfigflag) && WiFi.smartConfigDone()) {
					_wifiConfigCounter = 0;
				}
				if ((WIFI_WPSCONFIG == _wificonfigflag) && WIFI_WPSConfigDone()) {
					_wifiConfigCounter = 0;
				}
				if (!_wifiConfigCounter) {
					if (strlen(WiFi.SSID().c_str())) {
						strlcpy(settings.wifi_ssid[0], WiFi.SSID().c_str(), sizeof(settings.wifi_ssid[0]));
					}
					if (strlen(WiFi.psk().c_str())) {
						strlcpy(settings.wifi_pwd[0], WiFi.psk().c_str(), sizeof(settings.wifi_pwd[0]));
					}
					//sysCfg.sta_active = 0;
					Serial.printf("WIF: SmartConfig SSId1 %s, Password1 %s\n", settings.wifi_ssid[0], settings.wifi_pwd[0]);
				}
			}
			if (!_wifiConfigCounter) {
				if (WIFI_SMARTCONFIG == _wificonfigflag) {
					WiFi.stopSmartConfig();
				}
				restartflag = 2;
			}
		}
		else {
			if (_wificounter <= 0) {
				Serial.print("WIF: Checking connection...\n");
				_wificounter = WIFI_CHECK_SEC;
				WIFI_check_ip();
			}
			if ((WL_CONNECTED == WiFi.status()) && (static_cast<uint32_t>(WiFi.localIP()) != 0) && !_wificonfigflag) {
				ticker.detach(); // stop ticker couse you have connected to the WiFi
#ifdef USE_DISCOVERY
				if (!mDNSbegun) {
					mDNSbegun = MDNS.begin(Hostname);
					Serial.printf("DNS: %s\n", (mDNSbegun) ? "Initialized" : "Failed");
				}
#endif  // USE_DISCOVERY
#ifdef USE_WEBSERVER
				if (settings.webserver) {
					startWebserver(settings.webserver, WiFi.localIP());
#ifdef USE_DISCOVERY
#ifdef WEBSERVER_ADVERTISE
					MDNS.addService("http", "tcp", 80);
#endif  // WEBSERVER_ADVERTISE
#endif  // USE_DISCOVERY
				}
				else {
					stopWebserver();
				}
#ifdef USE_EMULATION
				if (sysCfg.flag.emulation) {
					UDP_Connect();
				}
#endif  // USE_EMULATION
#endif  // USE_WEBSERVER
			}
			else {
#ifdef USE_EMULATION
				UDP_Disconnect();
#endif  // USE_EMULATION
				mDNSbegun = false;
			}
		}
	}
}

int WIFI_State()
{
	int state;

	if ((WL_CONNECTED == WiFi.status()) && (static_cast<uint32_t>(WiFi.localIP()) != 0)) {
		state = WIFI_RESTART;
	}
	if (_wificonfigflag) {
		state = _wificonfigflag;
	}
	return state;
}

void WIFI_Connect()
{
	// start ticker with 0.5 because we start in AP mode and try to connect
	ticker.attach(0.5, tick);

	WiFi.persistent(false);   // Solve possible wifi init errors
	_wifistatus = 0;
	_wifiretry = WIFI_RETRY_SEC;
	_wificounter = 1;
}
