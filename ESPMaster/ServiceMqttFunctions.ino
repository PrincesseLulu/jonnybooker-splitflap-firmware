// MQTT service integration for Split-Flap display
// Uses PubSubClient. Make sure the PubSubClient library is installed in Arduino IDE.

#include <PubSubClient.h>

// MQTT configuration constants are declared in ESPMaster.ino
extern const char* mqttBroker;
extern const int   mqttPort;
extern const char* mqttUsername; // can be empty
extern const char* mqttPassword; // can be empty
extern const char* mqttTopic;    // topic to subscribe to for text messages

extern bool isWifiConfigured; // from ESPMaster.ino
extern String inputText;      // current text to show
extern String deviceMode;     // device mode string (text/clock/date/countdown)
extern const char* DEVICE_MODE_TEXT; // symbol from ESPMaster.ino
extern Timezone timezone;     // for timestamping lastReceivedMessageDateTime
extern String lastReceivedMessageDateTime;

// Forward declaration of showText from ServiceFlapFunctions.ino
void showText(String message);

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);
static unsigned long lastMqttReconnectAttempt = 0;

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  message.reserve(length);
  for (unsigned int i = 0; i < length; i++) {
    message += static_cast<char>(payload[i]);
  }

  // Trim message but allow empty payloads to intentionally clear the display
  message.trim();
  if (message.length() == 0) {
    SerialPrintln("MQTT: Received empty message, will display blanks");
  }

  SerialPrint("MQTT: Message on topic: ");
  SerialPrintln(String(topic));
  SerialPrint("MQTT: Payload: ");
  SerialPrintln(message);

  // Update state to show message indefinitely like web text mode
  deviceMode = DEVICE_MODE_TEXT;
  inputText = message;
  lastReceivedMessageDateTime = timezone.dateTime("d M y H:i:s");
  showText(message);
}

static bool mqttConnect() {
  if (mqttClient.connected()) return true;

  String clientId = String("splitflap-") + String(ESP.getChipId(), HEX);

  bool connected = false;
  if (mqttUsername != nullptr && strlen(mqttUsername) > 0) {
    connected = mqttClient.connect(clientId.c_str(), mqttUsername, mqttPassword);
  } else {
    connected = mqttClient.connect(clientId.c_str());
  }

  if (connected) {
    SerialPrintln("MQTT: Connected to broker");
    // Subscribe to primary topic
    if (mqttTopic != nullptr && strlen(mqttTopic) > 0) {
      if (mqttClient.subscribe(mqttTopic)) {
        SerialPrint("MQTT: Subscribed to topic: ");
        SerialPrintln(String(mqttTopic));
      } else {
        SerialPrintln("MQTT: Failed to subscribe to topic");
      }
    }
  } else {
    SerialPrint("MQTT: Connect failed, state: ");
    SerialPrintln(mqttClient.state());
  }

  return connected;
}

void mqttSetup() {
  if (!isWifiConfigured) {
    SerialPrintln("MQTT: WiFi not configured, skipping MQTT setup");
    return;
  }
  mqttClient.setServer(mqttBroker, mqttPort);
  mqttClient.setCallback(mqttCallback);
  // Attempt initial connection
  mqttConnect();
}

void mqttLoop() {
  if (!isWifiConfigured) {
    return;
  }

  if (!mqttClient.connected()) {
    unsigned long now = millis();
    // Try reconnect every 5 seconds
    if (now - lastMqttReconnectAttempt > 5000) {
      lastMqttReconnectAttempt = now;
      mqttConnect();
    }
  } else {
    mqttClient.loop();
  }
}
