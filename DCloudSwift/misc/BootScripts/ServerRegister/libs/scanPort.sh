#!/bin/bash

SNMPWALK=/usr/bin/snmpwalk
SNMPCOMMUNITY=public
SNMPVERSION=1
#SWITCHADDR=172.16.14.246
#SWITCHADDR=10.129.7.230
SWITCHADDR=172.16.229.3
ALERTMAC="$1"
SNMPWALKS="$SNMPWALK -c$SNMPCOMMUNITY -v$SNMPVERSION"
FDBTABLE=1.3.6.1.2.1.17.7.1.2.2.1.2

if [ "$ALERTMAC" == "" ]; then
  exit 0
fi
  
libs/clearSwitchCache.exp $SWITCHADDR > /dev/null &
#echo "Switch: $SWITCHADDR"
#echo "$SNMPWALK -c$SNMPCOMMUNITY -v$SNMPVERSION $SWITCHADDR $FDBTABLE"
$SNMPWALK -c$SNMPCOMMUNITY -v$SNMPVERSION $SWITCHADDR $FDBTABLE| \
  sed "s/[\.:=]/ /g"| \
  awk 'function hex2str(hexcode) { \
    if(hexcode < 16) { \
      str=sprintf("0%x",hexcode); \
    } else { \
      str=sprintf("%x",hexcode); \
    } \
    return str; \
    } \
    { \
  port=$22; \
  mac1=hex2str($15); \
  mac2=hex2str($16); \
  mac3=hex2str($17); \
  mac4=hex2str($18); \
  mac5=hex2str($19); \
  mac6=hex2str($20); \
  if((port<=48 && port>0)) \
    printf("HWaddr %2s:%2s:%2s:%2s:%2s:%2s Port %2d\n",\
    mac1,mac2,mac3,mac4,mac5,mac6,port); \
  }' \
  |grep $ALERTMAC
