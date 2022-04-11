#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ESP32Servo.h> 
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "../config.h"

const char* ssid = SSID;
const char* password = PASSWORD;
const char* mqtt_server = MQTT_SERVER;

WiFiClient espClient;
PubSubClient client(espClient); //lib required for mqtt

Servo myservo;  // create servo object to control a servo

int servoPin = 18; 

long lastReconnectAttempt = 0;
long lastOvenAction = 0;

int servoPosOn = 1150;
int servoPosOff = 1800;
bool switchOven = false;

void callback (char *topic, byte *payload, unsigned int length);

void setup()
{
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  Serial.println("Wifi connected");
  client.setServer(mqtt_server, 1883);//connecting to mqtt server
  client.setCallback(callback);
  //delay(5000);

  lastReconnectAttempt = 0;

	// Allow allocation of all timers
	ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
  myservo.setPeriodHertz(50);// Standard 50hz servo
  myservo.attach(servoPin, 500, 2400);

  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
      ESP.restart();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

}

void callback (char *topic, byte *payload, unsigned int length) {   //callback includes topic and payload ( from which (topic) the payload is comming)
  myservo.attach(servoPin, 500, 2400);
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");

  char payloadString[20];

  memcpy(payloadString, payload, length);
  payloadString[length] = '\0';
  Serial.println(payloadString);

  // Process the command that was received.
  if (strcmp(topic, "basement/oven/set") == 0) {
    Serial.println("New message: ");
    int newServoPos = atoi(payloadString);
    if ((char)payload[0] == 'O' && (char)payload[1] == 'N')
    {
      myservo.writeMicroseconds(servoPosOn);
      client.publish("basement/oven/status", "Oven status ON.");
    }
    else if ((char)payload[0] == 'O' && (char)payload[1] == 'F')
    {
      myservo.writeMicroseconds(servoPosOff);
      client.publish("basement/oven/status", "Oven status OFF.");
    }
    else if ((char)payload[0] == 'S' && (char)payload[1] == 'W')
    {
      switchOven = true;
    }
    else if ((char)payload[0] == 'R' && (char)payload[1] == 'E')
    {
      Serial.println(myservo.readMicroseconds());
    }
    else
    {
      myservo.writeMicroseconds(newServoPos);
      client.publish("basement/oven/status", "Set the oven servo to new position.");
    }   
  }

  Serial.println();
}

boolean reconnect() {
  if (client.connect("ESP32_Oven")) {
    // Once connected, publish an announcement...
    client.publish("basement/oven/status","ESP32_Oven ONLINE");
    // ... and resubscribe
    client.subscribe("basement/oven/set");
  }
  return client.connected();
}

void loop() {
  if (!client.connected()) {
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 5000) {
        Serial.println("Trying to connect to MQTT...");
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
  client.loop();
  }
  
  if (switchOven)
  {
    short servoPos;
    unsigned long now = millis(); // Do a task only once in 5 seconds.
    if (now - lastOvenAction > 7000) {
      lastOvenAction = now;
      servoPos = myservo.readMicroseconds();
      Serial.println(servoPos);
      // Switch oven state.        
      if (servoPos <= 1500)
      {
        // Serial.println("Servo position is smaller than 1500, switching oven off.");
        client.publish("basement/oven/status", "Switching oven OFF");
        for (short i = servoPos; i <= servoPosOff; i++)
        {
          myservo.writeMicroseconds(i);
          delay(1);
        }
      }
      else if (servoPos > 1500) {
        //Serial.println("Servo position is greater than 1500, switching oven on.");
        client.publish("basement/oven/status", "Switching oven ON");
        for (short i = servoPos; i >= servoPosOn; i--)
        {
          myservo.writeMicroseconds(i);
          delay(1);
        }
      }
      Serial.println(myservo.readMicroseconds());
    }
    switchOven = false;
  }

  delay(200);

  ArduinoOTA.handle();
}
