#!/bin/bash

#[ -z ${MANAGEMENT_HOST} ] && echo " WARN!! ./mkramdisk.sh hostname or ./mkramdisk.sh ipaddress" && exit 1

IPLISTFILE=iplist

[ -e ${IPLISTFILE} ] && IPLIST=`cat iplist` || exit 1

for i in ${IPLIST}; do
	echo ${i}
	./mkramdisk.expect ${i}
	exit 0
	
done
