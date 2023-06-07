#include "config.h"

#ifdef IS_ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "time.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"


unsigned long lastSent; // last MQTT message published

// Create an ESP8266 WiFiClient class to connect to the MQTT server.
WiFiClient client;

// Setup the MQTT client class by passing in the WiFi client and MQTT server and login details.
Adafruit_MQTT_Client mqtt(&client, MQTT_SERVER, MQTT_SERVERPORT, MQTT_USERNAME, MQTT_KEY);

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
    #ifdef SLEEP_DONT_LOOP
      ESP.deepSleep(SLEEP_TIME * 10e5);//10e5 = 1 second
      delay(100);
    #else
      ESP.restart();
    #endif
  }
  Serial.println("MQTT Connected!");
}

void WiFi_connect() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWD);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Wifi connection failed! Aborting...");
    #ifdef SLEEP_DONT_LOOP
      ESP.deepSleep(SLEEP_TIME * 10e5);//10e5 = 1 second
      delay(100);
    #else
      ESP.restart();
    #endif
  }

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

String getTimeStr(){
  struct tm timeinfo;
  char buffer[40];
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  } else {
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  }
  return String(buffer);
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

/*************************************
 * SETUP - run always if SLEEP_DONT_LOOP is true or just once otherwise
 */
void setup() {
  #ifdef IS_ESP32
    analogReadResolution(10);
  #endif
  pinMode(LED_PIN, OUTPUT);      // declare LED as output

  #ifdef VOLTAGE_INPUT_PIN
    pinMode(VOLTAGE_INPUT_PIN, INPUT);   // declare battery voltage as input
  #endif
  #ifdef DATA_INPUT_PIN_1
    pinMode(DATA_INPUT_PIN_1, INPUT);     // declare sensor as input
  #endif
  #ifdef DATA_INPUT_PIN_2
    pinMode(DATA_INPUT_PIN_2, INPUT);     // declare sensor as input
  #endif
  #ifdef DATA_INPUT_PIN_3
    pinMode(DATA_INPUT_PIN_3, INPUT);     // declare sensor as input
  #endif

  Serial.begin(115200);
  Serial.println("Booting");

  WiFi_connect();
  configTime(0, 0, "pool.ntp.org");

  #ifdef SLEEP_DONT_LOOP
    collectAndPublish();
    delay(100); // gives time to complete whatever task is pending
    
    Serial.print(F("Going to sleep for "));
    Serial.print(SLEEP_TIME);
    Serial.println(F(" seconds..."));
    
    mqtt.disconnect();
    WiFi.disconnect();
    ESP.deepSleep(SLEEP_TIME * 10e5);//10e5 = 1 second
    // goes to sleep and runs the setup() again on wakeup
    delay(100);
  #endif

  #ifdef OTA_PORT
    setupOTA();
  #endif
  // ... and continues to the loop()
}

void collectAndPublishHumidity(int pin, int air_value, int water_value, char* mqtt_channel) {
  char buf[256];
  String message;  
  int soilMoistureValue = 0;
  int soilMoisturePercent = 0;
  int sumValues = 0;
  Adafruit_MQTT_Publish mqtt_client_data = Adafruit_MQTT_Publish(&mqtt, mqtt_channel);

  String strNow = getTimeStr();
  Serial.print( "Now: " );
  Serial.println( strNow );

  for( int i=0; i<N_MEASUREMENTS; i++) {
    sumValues = sumValues + analogRead(pin);
    delay( MEASUREMENTS_DELAY );
  }
  soilMoistureValue = sumValues / N_MEASUREMENTS;
    
  soilMoisturePercent = map(soilMoistureValue, air_value, water_value, 0, 100);
  if (soilMoisturePercent < 0) soilMoisturePercent=0;
  if (soilMoisturePercent > 100) soilMoisturePercent=100;
  Serial.print("Pin ");
  Serial.print(pin);
  Serial.print(" humidity read: ");
  Serial.print(soilMoistureValue);
  Serial.print(", ");
  Serial.print(soilMoisturePercent);
  Serial.println("%");
  if( (soilMoistureValue >= MIN_VALID) && (soilMoistureValue <= MAX_VALID) ) {
    message = "{\"humidity\": " + String(soilMoisturePercent) + ", \"measurement\": " + String(soilMoistureValue) +", \"time\": \"" + strNow + "\"}";
  } else {
    message = "{\"humidity\": null, \"measurement\": " + String(soilMoistureValue) +", \"time\": \"" + strNow + "\"}";
  }
  message.toCharArray(buf, 256);
  if (!mqtt_client_data.publish(buf)) {
    Serial.println(F("Failed publishing MQTT message"));
  } else {
    Serial.println(F("Success publishing MQTT message!"));
  }
  delay(100);
  }

void collectAndPublish() {
  char buf[256];
  String message;  
  digitalWrite(LED_PIN, LOW);
  
  MQTT_connect(); // just in case it disconnected
  
  #ifdef VOLTAGE_INPUT_PIN
    Adafruit_MQTT_Publish mqtt_client_voltage = Adafruit_MQTT_Publish(&mqtt, MQTT_CHANNEL_VOLTAGE);

    //Read battery voltage
    float voltage = ( analogRead(VOLTAGE_INPUT_PIN) * (VOLTAGE_SLOPE) + (VOLTAGE_INTERCEPT) );
    Serial.print("Voltage read: ");
    Serial.println(voltage);
    if (!mqtt_client_voltage.publish(voltage)) {
      Serial.println(F("Failed publishing MQTT message"));
    } else {
      Serial.println(F("Success publishing MQTT message!"));
    }
    delay(100);
  #endif
  
  #ifdef DATA_INPUT_PIN_1
    collectAndPublishHumidity( DATA_INPUT_PIN_1, AIR_VALUE_1, WATER_VALUE_1, MQTT_CHANNEL_DATA_1);
  #endif

  #ifdef DATA_INPUT_PIN_2
    collectAndPublishHumidity( DATA_INPUT_PIN_2, AIR_VALUE_2, WATER_VALUE_2, MQTT_CHANNEL_DATA_2);
  #endif

  #ifdef DATA_INPUT_PIN_1
    collectAndPublishHumidity( DATA_INPUT_PIN_3, AIR_VALUE_3, WATER_VALUE_3, MQTT_CHANNEL_DATA_3);
  #endif
  
  digitalWrite(LED_PIN, HIGH);
}

/*****************************
 *  LOOP - only run when SLEEP_DONT_LOOP is false
 */
void loop() {
  unsigned long lastRun; // variable to store last run

  collectAndPublish();
  
  #ifdef OTA_PORT
    lastRun = millis();
    while( millis() < ( lastRun + SLEEP_TIME * 1000 ) ) { //waits in the OTA loop until it's time to continue
      ArduinoOTA.handle();
      delay(10);
    }
  #else
    delay( SLEEP_TIME * 1000 );
  #endif
  Serial.print(F("Next measurement in "));
  Serial.print(SLEEP_TIME);
  Serial.println(F(" seconds"));
}
