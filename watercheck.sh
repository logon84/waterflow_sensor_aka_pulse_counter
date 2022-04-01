#!/bin/sh
#Designed for Openwrt. Cron this every 15min

my_smartphone_mac="01:02:03:04:05:06"
mqtt_user="mqtt_user_here"
mqtt_password="mqtt_password_here"
mqtt_broker="192.168.1.1"

export PATH="$PATH:/opt/bin:/opt/sbin:/opt/usr/bin:/opt/usr/sbin"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:/opt/lib:/opt/usr/lib"

amIpresent=$(arp -a | grep $my_smartphone_mac | tr -d ' ')
if [ -z $amIpresent ]; then
	#I'm out of home
	/opt/usr/bin/mosquitto_sub -h $mqtt_broker -u $mqtt_user -P $mqtt_password -t waterflow -q 2 -C 1 -W 5 > /opt/usr/sbin/waterflow_sensor.topic
	waterflow_status=$(cat /opt/usr/sbin/waterflow_sensor.topic)
	edges_count=$(cat /opt/usr/sbin/waterflow_sensor.topic | jq -r '.p_e' | cut -d "_" -f2)
	sensor_since=$(cat /opt/usr/sbin/waterflow_sensor.topic | jq -r '.since')
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
			sensor_flow=$(cat /opt/usr/sbin/waterflow_sensor.topic | jq -r '.flow')
			echo $edges_count > /opt/usr/sbin/waterflow_sensor.edges
			if [ "$sensor_since" = "$sensor_since_prev" ]; then
				if [ $edges_count -ne $edges_count_prev ]; then
				#new edges detected, but I'm out of home. Water running!
					if [ $(echo "$sensor_flow > 0.15" | bc -l) -eq 1 ]; then
						/opt/usr/bin/telegram-send --config /opt/etc/telegram-send.conf "ALERTA: Fuga de agua en casa! ($edges_count_prev, $edges_count) at $sensor_flow L/min "
					else
						if [ -e /opt/usr/sbin/waterflow_sensor.strikes ]; then
							strikes=$(cat /opt/usr/sbin/waterflow_sensor.strikes)
							strikes=$(($strikes+1))
						else
							strikes="1"
						fi
						echo $strikes > /opt/usr/sbin/waterflow_sensor.strikes
						if [ $strikes -gt 2 ]; then
							/opt/usr/bin/telegram-send --config /opt/etc/telegram-send.conf "ALERTA: Fuga de agua en casa! ($edges_count_prev, $edges_count) at $sensor_flow L/min "
							
						fi
					fi
				else
					echo "0" > /opt/usr/sbin/waterflow_sensor.strikes	
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
	if [ -e /opt/usr/sbin/waterflow_sensor.topic ]; then
		rm -rf  /opt/usr/sbin/waterflow_sensor.topic
	fi
	if [ -e /opt/usr/sbin/waterflow_sensor.strikes ]; then
		rm -rf  /opt/usr/sbin/waterflow_sensor.strikes
	fi
fi

#log status every hour
curr_min=$(date +'%M')
if [ "$curr_min" = "00" ]; then
	waterflow_status_live=$(/opt/usr/bin/mosquitto_sub -h $mqtt_broker -u $mqtt_user -P $mqtt_password -t waterflow -q 2 -C 1 -W 5)
	curr_date=$(date +'%Y-%m-%d--%H:%M:%S')
	echo "$curr_date: $waterflow_status_live" >> /opt/var/log/mosquitto.log
fi

