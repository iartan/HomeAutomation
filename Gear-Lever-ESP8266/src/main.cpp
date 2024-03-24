#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

#include "../config.h"

#define LEFT 14 // D1 (GPIO5)
#define TOP 4 // D2 (GPIO4)
// Change bottom and right pins because bottom pin doesnotworkanymore on  the gear lever.
// So the right will be  botto for now.  
// Right needs to be 5 and bottom 12 under normal circumstances.
#define BOTTOM 5 // D5 (GPIO14)
#define RIGHT 12 // D6 (GPIO12)

const char* ssid = SSID;
const char* password = PASSWORD;
const char* mqtt_server = MQTT_SERVER;

WiFiClient espClient;
PubSubClient client(espClient);

uint8_t zeroState;
uint8_t threeState;
uint8_t oneState;
uint8_t ovenState;
uint8_t currentState = 5;

uint8_t topState;
uint8_t rightState;
uint8_t bottomState;
uint8_t leftState;

long lastOvenAction = 0;
long lastMsg = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Handle message arrived
}

unsigned long lastReconnectAttempt = 0;

boolean reconnect() {
  if (client.connect("GearLeverClient")) {
    client.publish("GearLever", "Gear lever online");
    client.subscribe("inTopic");
  }
  return client.connected();
}

void checkState() {
  if (zeroState == 1) {
    currentState = 0;
  }
  else if (threeState == 1) {
    currentState = 3;
  }
  else if (oneState == 1) {
    currentState = 1;
  }
}

void setup() { 
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  ArduinoOTA.onStart([]() {
  String type;
  if (ArduinoOTA.getCommand() == U_FLASH) {
    type = "sketch"; 
  }
  else { // U_FS
  type = "filesystem";
  }
  // NOTE: if updating FS this would be the place to unmount FS using FS.end()
  Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();

  pinMode(TOP, INPUT);
  pinMode(RIGHT, INPUT);
  pinMode(BOTTOM, INPUT);
  pinMode(LEFT, INPUT);

  lastReconnectAttempt = 0;
}

void loop() {
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    // Client connected
    client.loop();
  }

  unsigned long now = millis(); // Do a task only once in 1.5 seconds.
  if (now - lastOvenAction > 1000) {
    lastOvenAction = now;
    if (ovenState == 1) {
      client.publish("basement/oven/set", "SWITCH");
      delay(50);
    }
  }

  // zeroState = digitalRead(TOP);
  // threeState = digitalRead(RIGHT);
  // oneState = digitalRead(BOTTOM);
  // ovenState = digitalRead(LEFT);

  topState = digitalRead(TOP);  // Value 1 for currentState.
  rightState = digitalRead(RIGHT);  // Value 2 for currentState.
  bottomState = digitalRead(BOTTOM);  // Value 3 for currentState.
  leftState = digitalRead(LEFT);  // Value 4 for currentState.
  // currentState = 5; // Value for no state at all.

  unsigned long time = millis(); // Do a task only once in 1.5 seconds.
  if (time - lastMsg > 200) {
    lastMsg = time;
    if (topState == 1 && currentState != 1)
    {
      currentState = 1;
      client.publish("Top", "0");
      client.publish("kitchen/value/set", "80");
    }
    if (rightState == 1 && currentState != 2)
    {
      // currentState = 2;
      client.publish("Right", "0");
      client.publish("basement/oven/set", "SWITCH");
      delay(1000);
    }
    if (bottomState == 1 && currentState !=3)
    {
      currentState = 3;
      client.publish("Bottom", "0");
      client.publish("living/value/set", "80");
    }
    if (leftState == 1 && currentState != 4)
    {
      // currentState = 4;
      client.publish("Left", "0");
    }
    if (!topState && !rightState && !bottomState && !leftState && currentState != 5)
    {
      currentState = 5;
      client.publish("Zero", "0");
      client.publish("living/value/set", "0");
      client.publish("kitchen/value/set", "0");
    }
  }

  
  

  // if (currentState == 5)
  // {
  //   checkState();
  // }
  
  // if (zeroState == 1 && currentState != 0)
  // {
  //   client.publish("living/value/set", "0");
  //   client.publish("kitchen/value/set", "0");
  //   currentState = 0;
  //   delay(500);
  // }
  // else if (threeState == 1 && currentState != 3)
  // {
  //   client.publish("kitchen/value/set", "80");
  //   currentState = 3;
  //   delay(500);
  // }
  // else if (oneState == 1 && currentState != 1) {
  //   client.publish("living/value/set", "80");
  //   currentState = 1;
  //   delay(500);
  // }

  ArduinoOTA.handle();    
}