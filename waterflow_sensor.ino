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
*       liters - pulses converted to liters.
*       flow - Last waterflow (liters/min) calculated between edges.
*       p_e - Pulses_Edges. Useful to check if disc is moving comparing old and actual value.
*       moved_in_30min - '1' if a transition in input pin has been detected in the last half hour.
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

static bool send_data = true;
static bool mov_30min = false;
static bool mov_48hrs = false;
char buffer[256];
time_t Bootdatetime;
time_t Edgedatetime;

volatile bool edge_detected = false;
volatile bool pulse_reference; //indicates which kind of edge is going to be the reference for 1 pulse rev. (true = rising edge, false = falling edge)
volatile unsigned int Pulses = 0;
volatile unsigned int EdgesH = 0;
volatile unsigned int EdgesL = 0;
volatile unsigned int EdgeCurrent = 0;
volatile unsigned long LastEdgeMillis = 0;
volatile unsigned long EdgeDelta_ms = 0;

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  //Not used
}

WiFiClient WifiClient;
PubSubClient MqttClient(mqtt_broker, mqtt_port, mqttCallback, WifiClient);

void ICACHE_RAM_ATTR edgeHandler() {
  EdgeCurrent = bool(digitalRead(PULSE_PIN));
  if(EdgesH+EdgesL == 0) {
    pulse_reference = !EdgeCurrent;
  }
  if((unsigned long)(millis() - LastEdgeMillis) >= DEBOUNCE_MS) {
    edge_detected = true;
    if(EdgeCurrent) {
       EdgesH = EdgesH + 1;  
    }
    else {
      EdgesL = EdgesL + 1;
    }
    EdgeDelta_ms = (unsigned long)(millis() - LastEdgeMillis);
    LastEdgeMillis = millis();
    if(pulse_reference == EdgeCurrent) { //We got a pulse (1 rev)
      Pulses = Pulses + 1;
    }
  }
}

void truncate_float2buffer(float inp, int ndec) {
  if(ndec == 0) {
    sprintf(buffer, "%d", int(inp));
  }
  else {
    sprintf(buffer, "%f", inp);
    int i=0;
    while(buffer[i] != '.') {
      i++;
    }
    buffer[i+1+ndec] = '\0'; //Truncate float output to n decimals
  }
}

void insert_int_comma2buffer(int num, int pos) { //divide num by 10^pos by moving comma X positions to avoid float precision errors like 0,999999 and so
  int i;
  int len;
  sprintf(buffer, "%d", num);
  if(pos > 0) {
    len = strlen(buffer);
    buffer[len] = '_';
    buffer[len+1] = '\0';
    for(i=len;i+pos>len;i=i-1) {
      buffer[i] = buffer[i-1];
    }
    buffer[len-pos] = '.';
  }
}

time_t get_epoch_time() {
  time_t tnow = 0;
  tnow = time(nullptr);
  while (tnow<100000 || tnow == NULL) {
    tnow = time(nullptr);
    delay(100);
  }
  return tnow;
}

void formatted_time2buffer(time_t atime) {
  sprintf(buffer,ctime(&atime));
  buffer[strlen(buffer)-1] = '\0'; //Remove newline at the end of ctime output string
  return;
}

bool pubdata(void) {
  StaticJsonDocument<256> payload;
  insert_int_comma2buffer(int(10*EdgesL*FALLING_EDGE_LITERS) + int(10*EdgesH*RISING_EDGE_LITERS),1);
  payload["liters"] = buffer;
  if (EdgeDelta_ms > 0) {
    truncate_float2buffer(float(int(100*(int(EdgeCurrent)*RISING_EDGE_LITERS + int(!EdgeCurrent)*FALLING_EDGE_LITERS)/float(EdgeDelta_ms/60000.0))/100.0),2);
    payload["flow"] = buffer;
  }
  else {
     payload["flow"] = "0";
  }
  sprintf(buffer, "%d", Pulses);
  sprintf(buffer + strlen(buffer), "_");
  sprintf(buffer + strlen(buffer), "%d", EdgesH+EdgesL);
  payload["p_e"]                  = buffer;
  payload["moved_in_30min"]       = int(mov_30min);
  payload["inactive_for_48hrs"]   = int(!mov_48hrs);
  if(EdgesH+EdgesL == 0) {
    payload["last_edge"]            = "-";
  }
  else {
    formatted_time2buffer(Edgedatetime);
    payload["last_edge"]          = buffer;
  }
  formatted_time2buffer(Bootdatetime);
  payload["since"]                = buffer;

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
  attachInterrupt(digitalPinToInterrupt(PULSE_PIN), edgeHandler, CHANGE);

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
    if(mov_30min && (unsigned long)(millis() - LastEdgeMillis) >= 1800 * 1000) {
      mov_30min = false;
      send_data = true;
    }
    if(mov_48hrs && (unsigned long)(millis() - LastEdgeMillis) >= 3600 * 24 * 2 * 1000) {
      mov_48hrs = false;
      send_data = true;
    }
    if(edge_detected) {
      mov_30min = true;
      mov_48hrs = true;
      Edgedatetime = get_epoch_time();
      digitalWrite(blueLedPin, HIGH); //turnoff led
      send_data = true;
      edge_detected = false;
    }
    if(MqttClient.connected()) {
      if(send_data && (unsigned long)(millis() - LastEdgeMillis) >= MIN_TIME_BETWEEN_SENDS_MS && pubdata()) {
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
