#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESP8266WiFi.h>
// #define FASTLED_INTERRUPT_RETRY_COUNT 0
#include <FastLED.h>
#include <PubSubClient.h>
#include <time.h>
#include <TZ.h>
#include <FS.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>

#include "../../config_fallback.h"

#define HOSTNAME                      "LivingRoom HOSTNAME"
#define AVAILABILITY_TOPIC          "LivingRoom Lighting"
#define LIVING_STATE              "living/value/status"
#define LIVING_COMMAND            "living/value/set"
#define KITCHEN_STATE             "kitchen/value/status"
#define KITCHEN_COMMAND           "kitchen/value/set"
#define DATA_PIN    5
#define LED_TYPE    TM1812
#define COLOR_ORDER RGB
#define NUM_LEDS    600
#define BRIGHTNESS          255
#define FRAMES_PER_SECOND   60

const char* ssid = SSID;
const char* password = PASSWORD;
const char* mqtt_server = MQTT_SERVER;
const char* mqtt_user = MQTT_USER;
const char* mqtt_password = MQTT_PASSWORD;

// A single, global CertStore which can be used by all connections.
// Needs to stay live the entire time any of the WiFiClientBearSSLs
// are present.
BearSSL::CertStore certStore;

CRGB leds[NUM_LEDS];

WiFiClientSecure espClient;
PubSubClient * client;
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
int value = 0;

bool updateLiving = false;
bool updateKitchen = false;
int valueLiving = 0;
int valueKitchen = 0;
int kitchenOld = 0;

unsigned long lastReconnectAttempt = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, PASSWORD);

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

boolean reconnect() {
  if (client->connect("LivingRoom Lighting", mqtt_user, mqtt_password))
  {
    client->publish(AVAILABILITY_TOPIC, "LivingRoom Lighting online");
    client->subscribe(KITCHEN_COMMAND);
    client->subscribe(LIVING_COMMAND);
  }
  return client->connected();
}

void setDateTime() {
  // You can use your own timezone, but the exact time is not used at all.
  // Only the date is needed for validating the certificates.
  configTime(TZ_Europe_Berlin, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}

void setupOTA() {
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("LivingRoom Lighting");

  ArduinoOTA.onStart([]() { Serial.println("Starting"); });
  ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
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
}

void callback(char *topic, byte *payload, unsigned int length) {

  char payloadString[20];
 
  memcpy(payloadString, payload, length);
  payloadString[length] = '\0';
  Serial.println(payloadString);

  // Process the command that was received
  if (strcmp(topic, LIVING_COMMAND) == 0) {
    Serial.println("Processing the brightness...");
    updateLiving = true;
    valueLiving = atoi(payloadString);
  } 
  else if (strcmp(topic, KITCHEN_COMMAND) == 0) {
    Serial.println("Processing the kitchen brightness...");
    updateKitchen = true;
    kitchenOld = valueKitchen;
    valueKitchen = atoi(payloadString);
  }
}

void setup() {
  delay(500);
  Serial.begin(9600);
  
  LittleFS.begin();
  setup_wifi();
  setDateTime();

  int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.printf("Number of CA certs read: %d\n", numCerts);
  if (numCerts == 0) {
    Serial.printf("No certs found. Did you run certs-from-mozilla.py and upload the LittleFS directory before running?\n");
    return; // Can't connect to anything w/o certs!
  }

  BearSSL::WiFiClientSecure *bear = new BearSSL::WiFiClientSecure();
  // Integrate the cert store with this connection
  bear->setCertStore(&certStore);

  client = new PubSubClient(*bear);

  client->setServer(mqtt_server, 8883);
  client->setCallback(callback);

  setupOTA();

  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS)
      .setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.show(); // needed to reset leds to zero

  lastReconnectAttempt = 0;
}

void loop() {
  if (!client->connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
        Serial.print("Trying to connect to MQTT...");
        lastReconnectAttempt = now;
        // Attempt to reconnect
        if (reconnect()) {
            lastReconnectAttempt = 0;
        }
    }
  } 
  else 
  {
  // Client connected
  client->loop();
  }

  if (updateLiving) {
    updateLiving = false;
    client->publish(LIVING_STATE, String(valueLiving).c_str(), true);

    for(int x = 106; x < 400; x += 2) {
      leds[x] = CHSV(160, 255, valueLiving * 2.55);
    }
    FastLED.show();
  }

  // Comment following section for activating KitchenHarmony
  // /*
  if (updateKitchen) {
    updateKitchen = false;
    client->publish(KITCHEN_STATE, String(valueKitchen).c_str(), true);
  
    for(int y = 0; y < 105; y += 2) {
      leds[y] = CHSV(160, 255, valueKitchen * 2.55);
    }
    FastLED.show();
  }
  // */

  // Here comes KitchenHarmony
  /*
  if (updateKitchen) {
    updateKitchen = false;
    client.publish("Harmony", String("value is: " + valueKitchen).c_str(), true);
  }
  */
  ArduinoOTA.handle();

  delay(1000 / FRAMES_PER_SECOND);
}