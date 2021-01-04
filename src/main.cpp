#include <Arduino.h>
#ifdef ESP8266
  #include <ESP8266WiFi.h>
#endif
#ifdef ESP32
  #include <WiFi.h>
#endif

#include <SinricPro.h>
#include <SinricProThermostat.h>
#include <DHT.h>

#include "credentials.h"


const int BAUD_RATE = 9600;
const int EVENT_WAIT_TIME = 60000;
const int DHT_PIN = D5;
const int DHT_TYPE = DHT11;

DHT dht(DHT_PIN, DHT_TYPE);

bool powerState;
float setTemperature;
float actualTemperature;
float actualHumidity;
float lastTemperature;
float lastHumidity;
unsigned long lastEvent = (-EVENT_WAIT_TIME);


bool onPowerState(const String &deviceId, bool &state) {
  Serial.printf("Thermostat %s turned %s\r\n", deviceId.c_str(), state?"on":"off");
  powerState = state; 
  return true;
}

bool onTargetTemperature(const String &deviceId, float &temperature) {
  Serial.printf("Thermostat %s set temperature to %f\r\n", deviceId.c_str(), temperature);
  setTemperature = temperature;
  return true;
}

bool onAdjustTargetTemperature(const String & deviceId, float &temperatureDelta) {
  setTemperature += temperatureDelta;
  Serial.printf("Thermostat %s changed temperature about %f to %f", deviceId.c_str(), temperatureDelta, setTemperature);
  temperatureDelta = setTemperature;
  return true;
}

bool onThermostatMode(const String &deviceId, String &mode) {
  Serial.printf("Thermostat %s set to mode %s\r\n", deviceId.c_str(), mode.c_str());
  return true;
}

void handleTemperatureSensor() {
  unsigned long actualMillis = millis();
  if (actualMillis - lastEvent < EVENT_WAIT_TIME) {
    return;
  }

  actualTemperature = dht.readTemperature();
  actualHumidity = dht.readHumidity();

  if (isnan(actualTemperature) || isnan(actualHumidity)) {
    Serial.printf("DHT reading failed!\r\n");
    return;
  } 

  if (actualTemperature == lastTemperature || actualHumidity == lastHumidity) {
    return;
  }

  SinricProThermostat &myThermostat = SinricPro[THERMOSTAT_ID];
  bool success = myThermostat.sendTemperatureEvent(actualTemperature, actualHumidity);
  if (success) {
    Serial.printf("Temperature: %2.1f Celsius\tHumidity: %2.1f%%\r\n", actualTemperature, actualHumidity);
  } else {
    Serial.printf("Something went wrong...could not send Event to server!\r\n");
  }

  lastTemperature = actualTemperature;
  lastHumidity = actualHumidity;
  lastEvent = actualMillis;
}


void setupWiFi() {
  Serial.printf("\r\n[Wifi]: Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.printf(".");
    delay(250);
  }
  IPAddress localIP = WiFi.localIP();
  Serial.printf("connected!\r\n[WiFi]: IP-Address is %d.%d.%d.%d\r\n", localIP[0], localIP[1], localIP[2], localIP[3]);
}

void setupSinricPro() {
  SinricProThermostat &myThermostat = SinricPro[THERMOSTAT_ID];
  myThermostat.onPowerState(onPowerState);
  myThermostat.onTargetTemperature(onTargetTemperature);
  myThermostat.onAdjustTargetTemperature(onAdjustTargetTemperature);
  myThermostat.onThermostatMode(onThermostatMode);

  // setup SinricPro
  SinricPro.onConnected([](){ Serial.printf("Connected to SinricPro\r\n"); }); 
  SinricPro.onDisconnected([](){ Serial.printf("Disconnected from SinricPro\r\n"); });
  SinricPro.begin(APP_KEY, APP_SECRET);
  SinricPro.restoreDeviceStates(true);
}

void setup() {
  Serial.begin(BAUD_RATE); Serial.printf("\r\n\r\n");
  dht.begin();
  setupWiFi();
  setupSinricPro();
}

void loop() {
  SinricPro.handle();
  handleTemperatureSensor();
}
