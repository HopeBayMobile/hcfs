#!/bin/bash

grep "glusterd is alive" log | awk ' { print $1 }' > iplist
IPLIST=`cat iplist`
SLEEP=3

#echo ${IPLIST}
#exit 0

for i in ${IPLIST}; do
	DATE=`date +%Y%m%d%H%M%S`
	echo " ${DATE} probing ip ${i}"
	gluster peer probe ${i}
	sleep ${SLEEP}
	gluster peer status
done
