#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

#include "config.h"

#ifdef DATA_INPUT_PIN
  int inputPin = DATA_INPUT_PIN;  // choose the input pin (for temperature probe)
#endif
#ifdef VOLTAGE_INPUT_PIN
  int voltagePin = VOLTAGE_INPUT_PIN;  // choose the input pin (for battery voltage)
#endif

unsigned long lastSent; // last MQTT message published

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_KEY);

// Setup MQTT feeds for publishing.
#ifdef VOLTAGE_INPUT_PIN
  Adafruit_MQTT_Publish mqtt_client_voltage = Adafruit_MQTT_Publish(&mqtt, MQTT_CHANNEL_VOLTAGE);
#endif

#ifdef DATA_INPUT_PIN
  Adafruit_MQTT_Publish mqtt_client_data = Adafruit_MQTT_Publish(&mqtt, MQTT_CHANNEL_DATA);
#endif

// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  if (mqtt.connect()) { // connect will return 0 for connected
    Serial.print("MQTT connection failed:");
    Serial.println(mqtt.connectErrorString(ret));
    mqtt.disconnect();
    Serial.println("Aborting...");
    if( SLEEP_DONT_LOOP ) {
      ESP.deepSleep(SLEEP_TIME * 10e5);//10e5 = 1 second
      delay(1000);
    } else {
      ESP.restart();
    }
  }
  Serial.println("MQTT Connected!");
}

void WiFi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Wifi connection failed! Aborting...");
    if (SLEEP_DONT_LOOP) {
      ESP.deepSleep(SLEEP_TIME * 10e5);//10e5 = 1 second
      delay(1000);
    } else {
      ESP.restart();
    }
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

#ifdef OTA_PORT
void setupOTA() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPort(OTA_PORT);
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  ArduinoOTA.setPassword(OTA_PASSWD);

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
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
}
#endif

void setup() {
  pinMode(LED_PIN, OUTPUT);      // declare LED as output

  #ifdef VOLTAGE_INPUT_PIN
    pinMode(voltagePin, INPUT);   // declare battery voltage as input
  #endif
  #ifdef DATA_INPUT_PIN
    pinMode(inputPin, INPUT);     // declare sensor as input
  #endif

  Serial.begin(115200);
  Serial.println("Booting");

  WiFi_connect();

  #ifdef OTA_PORT
    setupOTA();
  #endif

  if( SLEEP_DONT_LOOP ) {
    collectAndPublish();
    delay(2000);
    mqtt.disconnect();
    delay(1000);
    Serial.println(F("Going to sleep..."));
    ESP.deepSleep(SLEEP_TIME * 10e5);//10e5 = 1 second
    delay(1000);
  }
}

void collectAndPublish() {
  int soilMoistureValue = 0;
  int soilMoisturePercent=0;
  MQTT_connect(); // just in case it disconnected

  digitalWrite(LED_PIN, HIGH);
  delay(500);
  digitalWrite(LED_PIN, LOW);
  
  #ifdef VOLTAGE_INPUT_PIN
    //Read battery voltage
    float voltage = ( analogRead(VOLTAGE_INPUT_PIN) * (VOLTAGE_SLOPE) + (VOLTAGE_INTERCEPT) );
    Serial.print("Voltage read: ");
    Serial.println(voltage);
    if (!mqtt_client_voltage.publish(voltage)) {
      Serial.println(F("Failed publishing MQTT message"));
    } else {
      Serial.println(F("Success publishing MQTT message!"));
    }
    delay(500);
  #endif
  
  #ifdef DATA_INPUT_PIN
    soilMoistureValue = analogRead(A0);  
    soilMoisturePercent = map(soilMoistureValue, AIR_VALUE, WATER_VALUE, 0, 100);
    Serial.print("Humidity read: ");
    Serial.print(soilMoistureValue);
    Serial.print(", ");
    Serial.print(soilMoisturePercent );
    Serial.println("%");
  
    if (!mqtt_client_data.publish(soilMoisturePercent)) {
      Serial.println(F("Failed publishing MQTT message"));
    } else {
      Serial.println(F("Success publishing MQTT message!"));
    }
  #endif
}

void loop() {
  unsigned long lastRun; // variable to store last run

  collectAndPublish();
  
  lastRun = millis();
  #ifdef OTA_PORT
    while( millis() < ( lastRun + SLEEP_TIME * 1000 ) ) {
      ArduinoOTA.handle();
      delay(100);
    }
  #else
    delay(SLEEP_TIME * 1000);
  #endif

}
