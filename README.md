# Waterflow sensor - pulse counter which sends data over MQTT (W.I.P.)

* Written in Arduino IDE.
* This counter uses interrupts on ESP-01's GPIO3.
* It is "talk-only" client. 
* It sends out data while edges detected.
* NTP sync.
* It keeps adding pulses/edges until poweroff/reset.

### MQTT Message consists of 
       pulses - number of pulses counted since last device boot.
       liters - pulses converted to liters.
       edges - number of rising and falling edges detected. Useful to check if disc is moving comparing old and actual value.
       moved_last_half_hour - '1' if a transition in input pin has been detected in the last half hour.
       inactive_for_48hrs - '1' if no new data in last 48 hours. Good to alarm if sensor is not registering data for a long period.
       last_edge - Last edge detected datetime.
       since - ESP8266 last boot date.
       
  
![Schematic](https://raw.githubusercontent.com/logon84/waterflow_sensor_aka_pulse_counter/main/schematic3.png)

Copyright (C) 2021 Rubén López <rubenlogon@yahoo.es>  

This library is free software. You may use/redistribute it under The MIT License terms. 
