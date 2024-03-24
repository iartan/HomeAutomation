#include <ESP8266WiFi.h>
#include <ClickEncoder.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>

#include "config.h"

#define ENC_L_PINA     D0
#define ENC_L_PINB     D2
#define ENC_L_BTN      D3
#define ENC_K_PINA     D4
#define ENC_K_PINB     D6
#define ENC_K_BTN      D7

#define ENCODER_STEPS_PER_NOTCH    4   // Change this depending on which encoder is used

const char* ssid = SSID;
const char* password = PASSWORD;
const char* mqtt_server = MQTT_SERVER;

WiFiClient espClient;
PubSubClient client(espClient);

ClickEncoder encL = ClickEncoder(ENC_L_PINA,ENC_L_PINB,ENC_L_BTN,ENCODER_STEPS_PER_NOTCH);
ClickEncoder encK = ClickEncoder(ENC_K_PINA,ENC_K_PINB,ENC_K_BTN,ENCODER_STEPS_PER_NOTCH);

uint8_t getLivingBrightness = constrain(getLivingBrightness, 0, 100);
uint8_t getKitchenBrightness = 0;
uint8_t livingBrightness = (0);
uint8_t kitchenBrightness = (0);

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // Make payload a string by NULL terminating it.
  getLivingBrightness = atoi((char *)payload);
}

unsigned long lastReconnectAttempt = 0;

boolean reconnect() {
  if (client.connect("DoubleDimmerClient")) {
    // Once connected, publish an announcement...
    client.publish("DoubleDimmer","Online");
    // ... and resubscribe
    client.subscribe("living/value/status");
    client.subscribe("kitchen/value/status");
  }
  return client.connected();
}

/* Trying to get non-blocking mqtt.
void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.publish("DoubleDimmer", "Online");
      client.subscribe("living/value/status");
      client.subscribe("kitchen/value/status");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
      ESP.restart();
    }
  }
}
*/

void setup() {
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  Serial.begin(9600);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }  
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
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  encL.setButtonHeldEnabled(true);
  encL.setDoubleClickEnabled(false);
  encK.setButtonHeldEnabled(true);
  encK.setDoubleClickEnabled(false);
  
  encL.setButtonOnPinZeroEnabled(true);

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

  static uint32_t lastService = 0;
  if (lastService + 1000 < micros()) {
    lastService = micros();                
    encL.service();
    encK.service();  
  }
 
  static int16_t last, value, last2, value2;
  value += encL.getValue();
  
  if (value != last) {
    //livingBrightness = getLivingBrightness;
    getLivingBrightness += ((value - last) * 10);
    last = value;
    if (getLivingBrightness > 100) {
      getLivingBrightness = 100;
      client.publish("living/value/set", String(getLivingBrightness).c_str(), true);
    }
    else if (getLivingBrightness < 0) {
      getLivingBrightness = 0;
      client.publish("living/value/set", String(getLivingBrightness).c_str(), true);
    }
    else{
      client.publish("living/value/set", String(getLivingBrightness).c_str(), true);
    }
    Serial.print("Encoder Living value: ");
    Serial.println(getLivingBrightness);
  }

  value2 += encK.getValue();

  if (value2 != last2) {
    getKitchenBrightness += ((value2 - last2) * 10);
    last2 = value2;
    if (getKitchenBrightness > 100) {
      getKitchenBrightness = 100;
      client.publish("kitchen/value/set", String(getKitchenBrightness).c_str(), true);
    }
    else if (getKitchenBrightness < 0) {
      getLivingBrightness = 0;
      client.publish("kitchen/value/set", String(getKitchenBrightness).c_str(), true);
    }
    else{
      client.publish("kitchen/value/set", String(getKitchenBrightness).c_str(), true);
    }
    Serial.print("Encoder Kitchen value: ");
    Serial.println(getKitchenBrightness);
  }

  ClickEncoder::Button b = encL.getButton();
  if (b != ClickEncoder::Open) {
    Serial.print("Button: ");
    //#define VERBOSECASE(label, output) case label: client.publish("first/encoder", (#output)); delay(1000); break;
    switch (b) {
      case ClickEncoder::Pressed:
        client.publish("Encoder/button", "Pressed");
        delay(500);
        break;
      case ClickEncoder::Held:
        client.publish("Encoder/button", "Hold");
        delay(1000);
        break;
      case ClickEncoder::Released:
        client.publish("Encoder/button", "Released");
        delay(500);
        break;
      case ClickEncoder::Clicked:
        if (getLivingBrightness < 1) {
          getLivingBrightness = 80;
          client.publish("living/value/set", String(getLivingBrightness).c_str(), true);
          Serial.println(getLivingBrightness);
          break;
        }
        else {
          getLivingBrightness = 0;
          client.publish("living/value/set", String(getLivingBrightness).c_str(), true);
          break;
        }
      case ClickEncoder::DoubleClicked:
        client.publish("Encoder/button", "DoubleClicked");
        delay(500);
        break;
      case ClickEncoder::Open:
        break;
      case ClickEncoder::Closed:
        break;
    }
  }
  
  ClickEncoder::Button b2 = encK.getButton();
  if (b2 != ClickEncoder::Open) {
    Serial.print("Button: ");
    //#define VERBOSECASE(label) case label: client.publish("SECOND/enc", (#label)); break;
    switch (b2) {
      case ClickEncoder::Pressed:
        client.publish("2Enc/button", "Pressed");
        break;
      case ClickEncoder::Held:
        client.publish("2Enc/button", "Hold");
        delay(1000);
        break;
      case ClickEncoder::Released:
        client.publish("2Enc/button", "Released");
        break;
      case ClickEncoder::Clicked:
        if (getKitchenBrightness < 1) {
          getKitchenBrightness = 80;
          client.publish("kitchen/value/set", String(getKitchenBrightness).c_str(), true);
          break;
        }
        else {
          getKitchenBrightness = 0;
          client.publish("kitchen/value/set", String(getKitchenBrightness).c_str(), true);
          break;
        }
      case ClickEncoder::DoubleClicked:
        client.publish("2Enc/button", String(ESP.getFreeHeap()).c_str());
        break;
      case ClickEncoder::Open:
        break;
      case ClickEncoder::Closed:
        break;
    }
  }

  ArduinoOTA.handle();    
}