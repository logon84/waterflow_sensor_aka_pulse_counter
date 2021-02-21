/***************************************************************************************** 
*
* Waterflow sensor (Pulse counter) V3.0
*
* Written in Arduino IDE.
* This counter uses interrupts on ESP-01's GPIO3.
* It is "talk-only" client. 
* It sends out data while pulses detected.
* NTP sync.
* It keeps adding pulses until poweroff/reset.
*
* MQTT Message consists of 
*       pulses - number of pulses counted since last device boot.
*       liters - pulses converted to liters.
*       edges - number of rising and falling edges detected. Useful to check if disc is moving comparing old and actual value.
*       moved_last_half_hour - '1' if a transition in input pin has been detected in the last half hour.
*       inactive_for_48hrs - '1' if no new data in last 48 hours. Good to alarm if sensor is not registering data for a long period.
*       last_edge - Last edge detected datetime.
*       since - Last booting datetime.
*
* Copyright (C) 2021 Rubén López <rubenlogon@yahoo.es>
* 
*
* This library is free software. You may use/redistribute it under The MIT License terms. 
*
*****************************************************************************************/
#include <Arduino.h>
#include <ArduinoJson.h>
#include <time.h>
#include <TZ.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include "waterflow_sensor.h"
#ifdef OTA
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#endif

bool send_data = true;
bool moved_in_last_half_hour = false;
bool moved_in_last_48hrs = false;
time_t Bootdatetime;
time_t Edgedatetime;

volatile bool edge_detected = false;
volatile bool pulse_reference; //indicates which kind of edge is going to be the reference for 1 pulse rev. (true = rising edge, false = falling edge)
volatile unsigned int Pulses = 0;
volatile unsigned int Edges = 0;
volatile unsigned long LastEdgeMillis;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  //Not used
}

WiFiClient WifiClient;
PubSubClient MqttClient(mqtt_broker, mqtt_port, mqttCallback, WifiClient);
const size_t bufferSize = JSON_OBJECT_SIZE(20);
DynamicJsonDocument payload(bufferSize);

void ICACHE_RAM_ATTR pulseHandler() {
  if(Edges == 0) {
    pulse_reference = !digitalRead(PULSE_PIN);
  }
  if((unsigned long)(millis() - LastEdgeMillis) >= DEBOUNCE_MS) {
    edge_detected = true;
    Edges = Edges + 1;
    LastEdgeMillis = millis();
    if(pulse_reference == digitalRead(PULSE_PIN)) { //We got a pulse (1 rev)
      Pulses = Pulses + 1;
    }
  }
}

time_t get_epoch_time() {
  time_t tnow = 0;
  tnow = time(nullptr);
  while (tnow<100000) {
    tnow = time(nullptr);;
    delay(100);
  }
  return tnow;
}

bool pubdata(void) {
  payload["pulses"]               = Pulses; // = disc revs
  payload["liters"]               = Edges*1000*WATERMETER_RESOLUTION_M3*(10/2); //my watermeter has a resolution of 0.0001m³/rev and half circle disc. 1 rev m³ = 10 x 0.0001. Half metal circle m³ res = (10 x 0.0001)/2. Half metal circle liters res = 1000 x (10 x 0.0001)/2 = 0.5l 
  payload["edges"]                = Edges;
  payload["moved_last_half_hour"] = int(moved_in_last_half_hour);
  payload["inactive_for_48hrs"]   = int(!moved_in_last_48hrs);
  payload["since"]                = ctime(&Bootdatetime);
  if(Edges == 0){
    payload["last_edge"]            = '-';
  }
  else{
    payload["last_edge"]            = ctime(&Edgedatetime);
  }

  char buffer[512];
  serializeJson(payload, buffer);
    
  if(!MqttClient.publish(dest_topic, buffer, true)) {
    //Fail to publish
    if(!MqttClient.connected()) {
      //Not connected to broker
      if(!MqttClient.connect(client_id, mqtt_username, mqtt_password)) {
        //Can't connect to broker - Maybe wifi down? restart loop
      }
    }
    return false;
  }
  else {
    return true;
  }
}

void setup() {
  //Serial.begin(115200, SERIAL8N1, SERIALTX_ONLY);
  pinMode(blueLedPin, OUTPUT);
  digitalWrite(blueLedPin, HIGH);
  pinMode(PULSE_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(PULSE_PIN), pulseHandler, CHANGE);

  WiFi.hostname(client_id);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    // wait 500ms, flashing the blue LED to indicate WiFi connecting...
    digitalWrite(blueLedPin, LOW);
    delay(250);
    digitalWrite(blueLedPin, HIGH);
    delay(250);
  }
  MqttClient.connect(client_id, mqtt_username, mqtt_password);
  configTime(TIME_ZONE, "es.pool.ntp.org");
  yield();
  Bootdatetime = get_epoch_time();
#ifdef OTA
  ArduinoOTA.setHostname(client_id);
  ArduinoOTA.setPassword(ota_password);
  ArduinoOTA.begin();
#endif
}

void loop() {
  if(WiFi.status() == WL_CONNECTED) {
    if((unsigned long)(millis() - LastEdgeMillis) >= LED_FLICKER_MS){
      digitalWrite(blueLedPin, LOW); //There's wifi, then led ON
    }
    if(moved_in_last_half_hour && (unsigned long)(millis() - LastEdgeMillis) >= 1800 * 1000) {
      moved_in_last_half_hour = false;
      send_data = true;
    }
    if(moved_in_last_48hrs && (unsigned long)(millis() - LastEdgeMillis) >= 3600 * 24 * 2 * 1000) {
      moved_in_last_48hrs = false;
      send_data = true;
    }
    if(edge_detected) {
      moved_in_last_half_hour = true;
      moved_in_last_48hrs = true;
      Edgedatetime = get_epoch_time();
      digitalWrite(blueLedPin, HIGH); //turnoff led
      send_data = true;
      edge_detected = false;
    }
    if(MqttClient.connected()) {
      if(send_data && pubdata()) {
        send_data = false;
      }
      MqttClient.loop();
    }
    else {
      if(MqttClient.connect(client_id, mqtt_username, mqtt_password)) {
        MqttClient.loop();
      }
    }
  } 
  else {
    digitalWrite(blueLedPin, HIGH); //No wifi, no led light
    WiFi.begin(ssid, password);
    while(WiFi.waitForConnectResult() != WL_CONNECTED);
  }
#ifdef OTA
  ArduinoOTA.handle();
#endif
}
