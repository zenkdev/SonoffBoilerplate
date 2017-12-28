#pragma once

#define BUTTOM_DEBOUNCE        50                // Debounce time for buttons and sensors

#define HOSTNAME               "sonoff"
#define LEDMODE                LED_SYSTEM_RELAY  // [LedMode] Function of led (LED_OFF, LED_SYSTEM, LED_SYSTEM_RELAY, LED_SYSTEM_SENSOR)
#define HOLD_TIME              0                 // [HoldTime] Relay hold time in seconds, 0 - off
#define TELE_PERIOD            300               // [TelePeriod] Telemetry (0 = disable, 10 - 3600 seconds)

// -- Wifi ----------------------------------------
#define WIFI_HOSTNAME          HOSTNAME "-%04d"  // Expands to <HOSTNAME>-<last 4 decimal chars of MAC address>

#define WIFI_SSID1             "WirelessNet"     // [Ssid1] Wifi SSID
#define WIFI_PASS1             "4997818762"      // [Password1] Wifi password
#define WIFI_SSID2             "WirelessNet2"    // [Ssid2] Optional alternate AP Wifi SSID
#define WIFI_PASS2             "4997818762"      // [Password2] Optional alternate AP Wifi password
#define WIFI_CONFIG_TOOL       WIFI_MANAGER      // [WifiConfig] Default tool if wifi fails to connect (WIFI_RESTART, WIFI_SMARTCONFIG, WIFI_MANAGER, WIFI_WPSCONFIG, WIFI_RETRY, WIFI_WAIT)


// -- MQTT ----------------------------------------
#define USE_MQTT                                 // comment out to completly disable respective technology

#define MQTT_HOST              "192.168.100.3"   // [Mqtt] Host
#define MQTT_PORT              1883              // [Mqtt] MQTT port (10123 on CloudMQTT)
#define MQTT_USER              "SONOFF_USER"     // [Mqtt] Optional user
#define MQTT_PASS              "SONOFF_PASS"     // [Mqtt] Optional password
#define MQTT_TOPIC             HOSTNAME          // [Mqtt] (unique) MQTT device topic
#define MQTT_CLIENT_ID         "ESP8266_%06X"    // [Mqtt] Client ID. Also fall back topic using Chip Id = last 6 characters of MAC address
#define MQTT_RETRY_SECS        10                // Minimum seconds to retry MQTT connection
#define MQTT_STATUS_OFF        "OFF"             // [StateText1] Command or Status result when turned off (needs to be a string like "0" or "Off")
#define MQTT_STATUS_ON         "ON"              // [StateText2] Command or Status result when turned on (needs to be a string like "1" or "On")

#define SUB_PREFIX             "cmnd"            // [Prefix1] Sonoff devices subscribe to %prefix%/%topic% being SUB_PREFIX/MQTT_TOPIC and SUB_PREFIX/MQTT_GRPTOPIC
#define PUB_PREFIX             "stat"            // [Prefix2] Sonoff devices publish to %prefix%/%topic% being PUB_PREFIX/MQTT_TOPIC
#define PUB_PREFIX2            "tele"            // [Prefix3] Sonoff devices publish telemetry data to %prefix%/%topic% being PUB_PREFIX2/MQTT_TOPIC/UPTIME, POWER and TIME

#define TOPSZ                  100               // Max number of characters in topic string
#define CMDSZ                  20                // Max number of characters in command

#ifndef MESSZ
#define MESSZ                  368               // Max number of characters in JSON message string (4 x DS18x20 sensors)
#endif

// -- BLYNK ---------------------------------------
#define USE_BLYNK                                // comment out to completly disable respective technology

#define BLYNK_SERVER           "blynk-cloud.com" // [Blynk] Host
#define BLYNK_PORT             8442              // [Blynk] port
#define BLYNK_TOKEN            ""                // [Blynk] (unique) token
#define BLYNK_ANALOG_PIN       10                // [Blynk] Analog virtual pin
#define BLYNK_RETRY_SECS       10                // [Blynk] Minimum seconds to retry connection


// -- HTTP ----------------------------------------
#define USE_WEBSERVER                            // Enable web server and wifi manager (+62k code, +8k mem) - Disable by //

#define WEB_SERVER             2                 // [WebServer] Web server (0 = Off, 1 = Start as User, 2 = Start as Admin)
#define WEB_PORT               80                // Web server Port for User and Admin mode

#define INPUT_BUFFER_SIZE      250               // Max number of characters in (serial) command buffer

// -- mDNS ----------------------------------------
#define USE_DISCOVERY                            // Enable mDNS for the following services (+8k code, +0.3k mem) - Disable by //

#define WEBSERVER_ADVERTISE                      // Provide access to webserver by name <Hostname>.local/
#define MQTT_HOST_DISCOVERY                      // Find MQTT host server (overrides MQTT_HOST if found)


/*********************************************/
/* Should not need to edit below this line   */
/*********************************************/
#define MAX_RELAY              4
#define MAX_SENSOR             4
#define EEPROM_SALT            12666

typedef struct {
	// common
	byte module = 0;
	char hostname[33] = WIFI_HOSTNAME;
	uint8_t wifi_config = WIFI_CONFIG_TOOL;
	byte wifi_active = 0;
	char wifi_ssid[2][33] = { WIFI_SSID1, WIFI_SSID2 };
	char wifi_pwd[2][65] = { WIFI_PASS1, WIFI_PASS2 };
	uint8_t webserver = WEB_SERVER;
	char blynk_token[33] = BLYNK_TOKEN;
	char blynk_server[33] = BLYNK_SERVER;
	int16_t blynk_port = BLYNK_PORT;
	char mqtt_host[33] = MQTT_HOST;
	int16_t mqtt_port = MQTT_PORT;
	char mqtt_user[33] = MQTT_USER;
	char mqtt_pwd[33] = MQTT_PASS;
	char mqtt_client_id[24] = MQTT_CLIENT_ID;
	char mqtt_topic[33] = HOSTNAME;

	// basic
	uint8_t button_pin = 0;        // button pin
	uint8_t led_pin = 2;           // led pin
	boolean led_inverted = true;   // if true - LOW for led on, otherwise HIGH for led on
	uint8_t led_mode = LEDMODE;    // LED mode
	int16_t hold_time = HOLD_TIME; // relay hold on time in seconds

	// relays
	short relay_pin[MAX_RELAY] = { NOT_A_PIN, NOT_A_PIN, NOT_A_PIN, NOT_A_PIN };

	// sensors
	uint8_t sensor_pin[MAX_SENSOR] = { 14, 12, 13, 15 };
	byte sensor_mode[MAX_SENSOR] = { NO_SENSOR, NO_SENSOR, NO_SENSOR, NO_SENSOR };

	uint16_t tele_period = TELE_PERIOD;

	int16_t salt = EEPROM_SALT;    // salt
} WMSettings;

#define MAX_BUTTON_COMMANDS  5  // Max number of button commands supported
const char commands[MAX_BUTTON_COMMANDS][14] PROGMEM = {
	"WifiConfig 1",   // Press button three times
	"WifiConfig 2",   // Press button four times
	"WifiConfig 3",   // Press button five times
	"Restart 1",      // Press button six times
	"Upgrade 1" };    // Press button seven times
const char wificfg[MAX_WIFI_OPTION][12] PROGMEM = {
	"Restart",
	"SmartConfig",
	"WifiManager",
	"WPSConfig",
	"Retry",
	"Wait" };
