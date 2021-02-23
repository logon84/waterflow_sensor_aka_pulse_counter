#!/bin/sh
#Designed for Openwrt. Cron this every 15min

my_smartphone_mac="01:23:45:67:89:ab"
mqtt_user="mqtt_user_here"
mqtt_password="mqtt_password_here"
mqtt_broker="192.168.1.1"

export PATH="$PATH:/opt/bin:/opt/sbin:/opt/usr/bin:/opt/usr/sbin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/opt/lib:/opt/usr/lib"

amIpresent=$(arp -a | grep $my_smartphone_mac | tr -d ' ')
if [ -z $amIpresent ]; then
	#I'm out of home
	/opt/usr/bin/mosquitto_sub -h $mqtt_broker -u $mqtt_user -P $mqtt_password -t waterflow -q 2 -C 1 -W 5 > /opt/usr/sbin/waterflow.topic
	waterflow_status=$(cat /opt/usr/sbin/waterflow.topic)
	edges_count=$(cat /opt/usr/sbin/waterflow.topic | jq -r '.edges')
	sensor_since=$(cat /opt/usr/sbin/waterflow.topic | jq -r '.since')
	sensor_ip=$(cat /tmp/dhcp.leases | grep waterflow | cut -d' ' -f3)
	isSensorpresent=$(arp -a | grep $sensor_ip | tr -d ' ')
	if [ ! -z $isSensorpresent ]; then
		if [ ! -e /opt/usr/sbin/waterflow_sensor.edges ]; then
			#first water check after leaving home
			echo $edges_count > /opt/usr/sbin/waterflow_sensor.edges
			echo $sensor_since > /opt/usr/sbin/waterflow_sensor.since
		else
			#subsequent checks after leaving home
			edges_count_prev=$(cat /opt/usr/sbin/waterflow_sensor.edges)
			sensor_since_prev=$(cat /opt/usr/sbin/waterflow_sensor.since)
			echo $edges_count > /opt/usr/sbin/waterflow_sensor.edges
			if [ "$sensor_since" = "$sensor_since_prev" ]; then
				if [ $edges_count -ne $edges_count_prev ]; then
					#new edges detected, but I'm out of home. Water running!
					/opt/usr/bin/telegram-send --config /opt/etc/telegram-send.conf "ALERTA: Fuga de agua en casa $((($edges_count*0.5/15)-($edges_count_prev*0.5/15))) litros/minuto"
				fi
			else
				#power failure while being out, update sensor last boot time
				echo $sensor_since > /opt/usr/sbin/waterflow_sensor.since
			fi
		fi
	else
		if [ -e /opt/usr/sbin/waterflow_sensor.notices ]; then
			notices=$(cat /opt/usr/sbin/waterflow_sensor.notices)
		else
			notices="0"
		fi
		if [ $notices -lt 3 ]; then
			/opt/usr/bin/telegram-send --config /opt/etc/telegram-send.conf "waterflow_sensor not sending data"
			notices=$(($notices+1))
			echo $notices > /opt/usr/sbin/waterflow_sensor.notices
		fi
	fi
else
	#I'm home, so delete flags
	if [ -e /opt/usr/sbin/waterflow_sensor.edges ]; then
		rm -rf  /opt/usr/sbin/waterflow_sensor.edges
	fi
	if [ -e /opt/usr/sbin/waterflow_sensor.notices ]; then
		rm -rf  /opt/usr/sbin/waterflow_sensor.notices
	fi
	if [ -e /opt/usr/sbin/waterflow_sensor.since ]; then
		rm -rf  /opt/usr/sbin/waterflow_sensor.since
	fi
	if [ -e /opt/usr/sbin/waterflow.topic ]; then
		rm -rf  /opt/usr/sbin/waterflow.topic
	fi
fi
