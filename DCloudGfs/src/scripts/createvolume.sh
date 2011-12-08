#!/bin/bash

IFCONFIG=/sbin/ifconfig

VOLUME="iostat"
#IPADDRESS=`ifconfig |grep "inet addr" | grep Bcast | awk ' { print $2 }' | cut -d ":" -f 2`
IPADDRESS=`${IFCONFIG} |grep "inet addr" | grep Bcast | awk ' { print $2 }' | cut -d ":" -f 2`
PORT=24007
Host=localhost
SUBNET1=`echo ${IPADDRESS}| cut -d "." -f 1`
SUBNET2=`echo ${IPADDRESS}| cut -d "." -f 2`
SUBNET3=`echo ${IPADDRESS}| cut -d "." -f 3`
SUBNET4=`echo ${IPADDRESS}| cut -d "." -f 4`

SUBNET="${SUBNET1}.${SUBNET2}.${SUBNET3}"

#echo ${SUBNET}

#for i in `seq 1 254`; do
#
#	./telexpectcheck.expect ${SUBNET}.${i} $PORT
#
#done

#exit 0


MOUNTPOINT="/mnt/iostat"
IPLIST=`cat iplist`

volume_remove () {
	rm -rf /tmp/tmp*
	rm -rf /tmp/brick*
	rm -rf /etc/glusterd/*
	#/etc/init.d/glusterd restart
	kill -9 `ps -efa|grep glusterd | awk ' { print $2 }' `
	/usr/sbin/glusterd --log-level=NONE
}

#	volume_create () {
#		for i in `seq 1 480`; do
#		mkdir -p /mnt/disk1/brick${i}
#		BRICK_LIST="${BRICK_LIST} ${IPADDRESS}:/mnt/disk1/brick${i}"
#		#echo ${BRICK_LIST} >/tmp/brick.list
#		done
#		gluster volume create ${VOLUME} ${BRICK_LIST}
#		gluster volume start ${VOLUME}
#	}

#volume_create () {
#	for i in `seq 1 480`; do
#	BRICK_LIST="${BRICK_LIST} ${IPADDRESS}:/mnt/disk1"
#	#echo ${BRICK_LIST} >/tmp/brick.list
#	done
#	gluster volume create ${VOLUME} ${BRICK_LIST}
#	gluster volume start ${VOLUME}
#}

volume_create () {
	for i in ${IPLIST}; do
	BRICK_LIST="${BRICK_LIST} ${i}:/mnt/disk1 ${i}:/mnt/disk2"
	done
	echo ${BRICK_LIST} >/tmp/brick.list

	gluster volume create ${VOLUME} ${BRICK_LIST}
}

volume_start () {
	gluster volume start ${VOLUME}
}

#volume_remove
volume_create
#sleep 10

exit 0
#volume_start
[ -d ${MOUNTPOINT} ] || mkdir ${MOUNTPOINT}
mount -t glusterfs /etc/glusterd/vols/${VOLUME}/${VOLUME}-fuse.vol ${MOUNTPOINT}
