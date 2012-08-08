#!/bin/bash

SNMPWALK=/usr/bin/snmpwalk
SNMPCOMMUNITY=public
SNMPVERSION=1
#SWITCHADDR=172.16.14.246
SWITCHADDR=$1
SNMPWALKS="$SNMPWALK -c$SNMPCOMMUNITY -v$SNMPVERSION"
FDBTABLE=1.3.6.1.2.1.2.2.1.6.417

if [ "$1" == "" ] ; then
  exit 0;
fi
#./clearSwitchCache.sh $SWITCHADDR > /dev/null &
#echo "Switch: $SWITCHADDR"
#echo "$SNMPWALK -c$SNMPCOMMUNITY -v$SNMPVERSION $SWITCHADDR $FDBTABLE"
$SNMPWALK -c$SNMPCOMMUNITY -v$SNMPVERSION $SWITCHADDR $FDBTABLE| \
  sed "s/[\.:=]/ /g"| \
  awk 'function hex2str(hexcode) { \
    if(hexcode < 16) { \
      echo hexcode; \
      str=sprintf("0%x",hexcode); \
    } else { \
      str=sprintf("%x",hexcode); \
    } \
    return str; \
    } \
    { \
  mac1=$13; \
  mac2=$14; \
  mac3=$15; \
  mac4=$16; \
  mac5=$17; \
  mac6=$18; \
  printf("HWaddr %2s:%2s:%2s:%2s:%2s:%2s\n",\
  mac1,mac2,mac3,mac4,mac5,mac6); \
  }' 
 
