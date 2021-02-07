#!/bin/sh
#Designed for Openwrt. Cron this every 15min

my_smartphone_mac = "01:23:45:67:89:ab"
mqtt_broker = "192.168.1.1"
mqtt_user = "mqtt_user_here"
mqtt_password = "mqtt_password_here"

export PATH="$PATH:/opt/bin:/opt/sbin:/opt/usr/bin:/opt/usr/sbin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/opt/lib:/opt/usr/lib"

#delete mosquitto log if size is 1GB
log_size=$(sfk list -kbytes /opt/var/log/mosquitto.log | tr -dc '0-9'; echo "")
if [[ $log_size -gt 1048576 ]]; then
	rm -rf /opt/var/log/mosquitto.log
fi

amIpresent=$(arp -a | grep $my_smartphone_mac | tr -d ' ')

if [ -z $amIpresent ]; then
	#I'm out of home
	waterflow_status=$(/opt/usr/bin/mosquitto_sub -h $mqtt_broker -u $mqtt_user -P mqtt_password -t waterflow -q 2 -C 1 -W 5 | jq -r ".moved_last_half_hour")
	if [ ! -z $waterflow_status ]; then
		if [ ! -e /opt/usr/sbin/waterflow_sensor.strike ]; then
			#first water check after leaving home
			if [ $waterflow_status -eq 1 ]; then
				echo "1" > /opt/usr/sbin/waterflow_sensor.strike
			else
				echo "0" > /opt/usr/sbin/waterflow_sensor.strike
			fi
			echo $waterflow_status > /opt/usr/sbin/waterflow_sensor.last
		else
			#subsequent checks after leaving home
			waterflow_sensor_last=$(awk 'NR==1 {print; exit}' /opt/usr/sbin/waterflow_sensor.last)
			if [ $waterflow_sensor_last -eq 0 ] && [ $waterflow_status -eq 0 ]; then
				#All ok at home, reset strikes
				echo "0" > /opt/usr/sbin/waterflow_sensor.strike
			elif [ $waterflow_sensor_last -eq 0 ] && [ $waterflow_status -eq 1 ]; then
				#Waterflow detected. Alarm!
				/opt/usr/bin/telegram-send --config /opt/etc/telegram-send.conf "ALERTA - ALERTA: Fuga de agua en casa"
				echo "3" > /opt/usr/sbin/waterflow_sensor.strike
			elif [ $waterflow_sensor_last -eq 1 ] && [ $waterflow_status -eq 0 ]; then
				#All ok at home. The last water I used is no longer registered
				echo "0" > /opt/usr/sbin/waterflow_sensor.strike
			elif [ $waterflow_sensor_last -eq 1 ] && [ $waterflow_status -eq 1 ]; then
				waterflow_strike=$(awk 'NR==1 {print; exit}' /opt/usr/sbin/waterflow_sensor.strike)
				waterflow_strike=$((waterflow_strike + 1))
				echo $waterflow_strike > /opt/usr/sbin/waterflow_sensor.strike
				if [ $waterflow_strike -ge 3 ]; then
					#still water running. 
					/opt/usr/bin/telegram-send --config /opt/etc/telegram-send.conf "ALERTA - ALERTA: Fuga de agua en casa"
		        	fi
		        else
		        	/opt/usr/bin/telegram-send --config /opt/etc/telegram-send.conf "waterflow_sensor sending incorrect data"
		        fi
			echo $waterflow_status > /opt/usr/sbin/waterflow_sensor.last
		fi
	else
		/opt/usr/bin/telegram-send --config /opt/etc/telegram-send.conf "waterflow_sensor not sending data"
	fi
else
	#I'm home, so delete flags
	if [ -e /opt/usr/sbin/waterflow_sensor.strike ]; then
		rm -rf  /opt/usr/sbin/waterflow_sensor.strike
	fi
	if [ -e /opt/usr/sbin/waterflow_sensor.last ]; then
		rm -rf  /opt/usr/sbin/waterflow_sensor.last
	fi

fi
