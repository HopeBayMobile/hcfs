#!/bin/sh

#######################################
# LED_status -
#
#    0 - booting - on|on|off
#    1 - upgrading - on|on|blink
#    2 - normal - on|off|off
#    3 - error - on|blink|off
#    4 - shutdown - on|off|on
#
#######################################

#find the upgrade_led pid
LED_pid=$(ps aux | grep -w "/etc/delta/LED_status" | head -1 | awk '{ print $2 }')
#the kill the process
kill -9 $LED_pid
#finally change LED status 
LED_status=$1
/etc/delta/LED_status $LED_status &

