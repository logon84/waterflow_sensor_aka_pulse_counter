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
	waterflow_status=$(/opt/usr/bin/mosquitto_sub -h 192.168.1.1 -u ea8500 -P ea8500 -t waterflow -q 2 -C 1 -W 5)
	edges_count=$(echo $waterflow_status | jq -r ".edges")
	if [ ! -z $waterflow_status ]; then
		if [ ! -e /opt/usr/sbin/waterflow_sensor.edges ]; then
			#first water check after leaving home
			echo $edges_count > /opt/usr/sbin/waterflow_sensor.edges
		else
			#subsequent checks after leaving home
			edges_count_prev=$(awk 'NR==1 {print; exit}' /opt/usr/sbin/waterflow_sensor.edges)
			echo $edges_count > /opt/usr/sbin/waterflow_sensor.edges
			if [ $edges_count -ne $edges_count_prev ]; then
				#new edges detected, but I'm out of home. Water running!
				/opt/usr/bin/telegram-send --config /opt/etc/telegram-send.conf "ALERTA - ALERTA: Fuga de agua en casa"
		        fi
		fi
	else
		/opt/usr/bin/telegram-send --config /opt/etc/telegram-send.conf "waterflow_sensor not sending data"
	fi
else
	#I'm home, so delete flags
	if [ -e /opt/usr/sbin/waterflow_sensor.edges ]; then
		rm -rf  /opt/usr/sbin/waterflow_sensor.edges
	fi
fi
