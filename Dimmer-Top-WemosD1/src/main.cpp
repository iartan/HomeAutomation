#include "NewEncoder.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include "config.h"

#define ENC_PINA     D5
#define ENC_PINB     D6
#define ENC_BTN      D7

// Constants
const char* ssid = SSID;
const char* password = PASSWORD;
const char* mqtt_server = MQTT_SERVER;

const int RESET_VALUE = -1;
const int DEBOUNCE_DELAY = 10;  // Assuming delay(10) is for debouncing
const int RIGHT_THRESHOLD = 1;
const int LEFT_THRESHOLD = 3;

// WiFi status flag
bool isConnected = false;

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMQTTCheck = 0;  // Store the last time we sent an MQTT message
const long MQTT_INTERVAL = 10000;  // Interval to send MQTT message (10 seconds)

// Callback function to handle incoming MQTT messages (not used in this example but necessary for the library)
void callback(char* topic, byte* payload, unsigned int length) {
  // Handle incoming messages if needed
}

// Encoder and State
struct EncoderState {
  NewEncoder encoder;
  int rightCounter = 0;
  int leftCounter = 0;
  bool lastButtonState = false;
  int prevSteps = 0;
} encoderState;

unsigned long lastCheckTime = 0;  // Store the last time we checked WiFi connection
const long CHECK_INTERVAL = 5000;  // Interval at which to check WiFi (5 seconds)

void checkWiFiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    if (isConnected) {  // If it was previously connected, show disconnected message
      Serial.println("WiFi disconnected. Trying to reconnect...");
      isConnected = false;
    }
    // Attempt to reconnect (non-blocking)
    if (WiFi.status() == WL_CONNECT_FAILED) {
      WiFi.begin(ssid, password);
    }
  } else {
    if (!isConnected) {  // If it was previously disconnected, show connected message
      Serial.print("Connected to WiFi. IP address: ");
      Serial.println(WiFi.localIP());
      isConnected = true;
    }
  }
}

unsigned long lastMQTTReconnectAttempt = 0;
const long MQTT_RECONNECT_INTERVAL = 10000;  // Try to reconnect every 10 seconds if not connected

void setupMQTT() { client.setServer(mqtt_server, 1883); client.setCallback(callback); }

void sendMQTTMessage(const char* topic, const char* message) {
    if (WiFi.status() == WL_CONNECTED && client.connected()) {
        client.publish(topic, message);
    }
}

void reconnectMQTT() {
  if (WiFi.status() == WL_CONNECTED && !client.connected()) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastMQTTReconnectAttempt > MQTT_RECONNECT_INTERVAL) {
      lastMQTTReconnectAttempt = currentMillis;  // Save the last attempt time
      
      if (client.connect("DIMMER_TOP")) {
        Serial.println("Connected to MQTT broker");
      } else {
        Serial.print("Failed to connect to MQTT broker, rc=");
        Serial.println(client.state());
      }
    }
  }
}

void checkAndSendMQTT() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnectMQTT();
    }
    client.loop();

    unsigned long currentMillis = millis();
    if (currentMillis - lastMQTTCheck >= MQTT_INTERVAL) {
      lastMQTTCheck = currentMillis;  // Update the last check time
      client.publish("dimmer_top/status", "alive");
    }
  }
}

void checkButtonPress() {
  bool currentButtonState = encoderState.encoder.ButtonPressed();

  if (!encoderState.lastButtonState && currentButtonState) {
    Serial.println("Pressed");
    sendMQTTMessage("dimmer_top/button", "Pressed");
  }

  if (encoderState.lastButtonState && !currentButtonState) {
    Serial.println("Released");
  }

  encoderState.lastButtonState = currentButtonState;
}

void checkRotation() {
  int currentSteps = encoderState.encoder.GetSteps();
  int deltaSteps = currentSteps - encoderState.prevSteps;

  if (deltaSteps > 0) {
    encoderState.rightCounter++;
    encoderState.leftCounter = RESET_VALUE;
    if (encoderState.rightCounter >= RIGHT_THRESHOLD) {
      Serial.println("Turned Right");
      encoderState.rightCounter = 0;
    }
  } else if (deltaSteps < 0) {
    encoderState.leftCounter++;
    encoderState.rightCounter = 0;
    if (encoderState.leftCounter >= LEFT_THRESHOLD) {
      Serial.println("Turned Left");
      encoderState.leftCounter = 0;
    }
  }

  encoderState.prevSteps = currentSteps;
}

void setup() {
  encoderState.encoder.begin(D5, D6, D7, 10);
  Serial.begin(9600);
  encoderState.prevSteps = encoderState.encoder.GetSteps();

  WiFi.begin(ssid, password);

  setupMQTT();
}

void loop() {
  encoderState.encoder.Update();
  checkButtonPress();
  checkRotation();

  // Check if it's time to verify WiFi connection
  unsigned long currentMillis = millis();
  if (currentMillis - lastCheckTime >= CHECK_INTERVAL) {
    lastCheckTime = currentMillis;  // Save the current time for next interval
    checkWiFiConnection();
  }

  checkAndSendMQTT();  // Check MQTT connection and send message if needed

  delay(DEBOUNCE_DELAY);
}