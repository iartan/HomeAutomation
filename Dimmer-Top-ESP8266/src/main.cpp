#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EncoderButton.h>

#include <config.h>

// Rotary Encoder Pins
#define CLK_PIN D2
#define DT_PIN D1
#define SW_PIN D3

// WiFi credentials
const char* ssid = SSID;
const char* password = PASSWORD;

// MQTT broker details
const char* mqtt_server = MQTT_SERVER; // Use any public broker or your own
const int mqtt_port = 1883;
const char* mqtt_user = ""; // If your broker needs username
const char* mqtt_pass = ""; // If your broker needs password
const char* mqtt_topic = "esp8266/test";

WiFiClient espClient;
PubSubClient client(espClient);

// Setup EncoderButton
EncoderButton eb1(CLK_PIN, DT_PIN, SW_PIN);

unsigned long previousMillis = 0;
const long interval = 10000;  // 10 seconds

void setup() {
  Serial.begin(9600);

  // Connecting to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Setting up MQTT
  client.setServer(mqtt_server, mqtt_port);
}

void sendMqttMessage() {
  char msg[50];
  snprintf(msg, sizeof(msg), "Hello from ESP8266! Time: %lu", millis());
  Serial.print("Publish message: ");
  Serial.println(msg);
  client.publish(mqtt_topic, msg);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect("ESP8266Client", mqtt_user, mqtt_pass)) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  // Ensure MQTT is connected
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sendMqttMessage();
  }
}