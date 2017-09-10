#ifdef INCLUDE_MQTT_SUPPORT

#include <PubSubClient.h>

#define TOPSZ 100 // Max number of characters in topic string

WiFiClient espClient;
PubSubClient mqttClient(espClient);

static bool MQTT_ENABLED  = true;

int lastMQTTConnectionAttempt = 0;

void mqttDataCb(char* topic, byte* data, unsigned int data_len) {

  char topicBuf[TOPSZ];
  char dataBuf[data_len + 1];

  strncpy(topicBuf, topic, sizeof(topicBuf));
  memcpy(dataBuf, data, sizeof(dataBuf));
  dataBuf[sizeof(dataBuf) - 1] = 0;

  Serial.print(topicBuf);
  Serial.print(" => ");
  Serial.println(topic);
  String sTopic = topic;
  String payload = dataBuf;

  if (sTopic == settings.mqttTopic) {
    Serial.println("exact match");
  } else if (sTopic.startsWith(settings.mqttTopic)) {
    Serial.println("for this device");
    sTopic = sTopic.substring(strlen(settings.mqttTopic) + 1);
    String channelString = getValue(sTopic, '/', 0);
    if (channelString.startsWith("channel-")) {
      channelString.replace("channel-", "");
      int channel = channelString.toInt();
      Serial.println(channel);
      if (payload == "on") {
        turnOn(channel);
      }
      if (payload == "off") {
        turnOff(channel);
      }
      if (payload == "toggle") {
        toggle(channel);
      }
      if (payload == "") {
        MQTT_update(channel);
      }
    } else if (channelString == "reset" && payload == "yes"){
      Serial.println("Resetting...");
      reset();
    }
  }
}

void MQTT_setup() {
  if (strlen(settings.mqttHostname) == 0) {
    MQTT_ENABLED = false;
  }
  if (MQTT_ENABLED) {
    mqttClient.setServer(settings.mqttHostname, atoi(settings.mqttPort));
    mqttClient.setCallback(mqttDataCb);
  }
}

void MQTT_loop() {
  if (MQTT_ENABLED) {
    if (!mqttClient.connected()) {
      if (lastMQTTConnectionAttempt == 0 || millis() > lastMQTTConnectionAttempt + 3 * 60 * 1000) {
        lastMQTTConnectionAttempt = millis();
        Serial.print("Trying to connect to mqtt...");
        if (mqttClient.connect(settings.mqttClientID)) {
          // Once connected, publish an announcement...
          mqttClient.publish(settings.mqttTopic, "hello world");
          char topic[TOPSZ];
          Serial.println("connected");
          sprintf(topic, "%s/+", settings.mqttTopic);
          mqttClient.subscribe(topic);
          Serial.println(topic);

          //TODO multiple relays
          MQTT_update(0);
        } else {
          Serial.println("failed");
        }
      }
    } else {
      mqttClient.loop();
    }
  }
}

void MQTT_update(int channel) {
  int state = digitalRead(SONOFF_RELAY_PINS[channel]);
  char topic[TOPSZ];
  char stateString[5];
  sprintf(topic, "stat/%s/POWER%d", settings.mqttTopic, channel+1);
  if ( channel >= SONOFF_AVAILABLE_CHANNELS) {
    strcpy(stateString, "NULL");
  } else if (state == 0) {
    strcpy(stateString, "OFF");
  } else {
    strcpy(stateString, "ON");
  }
  mqttClient.publish(topic, stateString);
}

#endif

