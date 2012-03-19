#!/bin/bash

[ -e ./iplist ] || exit 1
NEWIP_LIST=`cat iplist`
EXPORT1_BRICK1=/export1/brick1
EXPORT2_BRICK1=/export2/brick1
SLEEP120=120
SLEEP10=10
VOLUME="ntu-management"
MOUNTPOINT="/mnt/${VOLUME}"

volume_remove () {
	rm -rf /tmp/tmp*
	rm -rf /tmp/brick*
	rm -rf /etc/glusterd/*
	#/etc/init.d/glusterd restart
	kill -9 `ps -efa|grep glusterd | awk ' { print $2 }' `
	/usr/sbin/glusterd --log-level=NONE
}

volume_create () {
	for i in ${NEWIP_LIST}; do
	BRICK_LIST="${BRICK_LIST} ${i}:${EXPORT1_BRICK1} ${i}:${EXPORT2_BRICK1}"
	done
	echo ${BRICK_LIST} >/tmp/brick.list

	gluster volume create ${VOLUME} ${BRICK_LIST}
}

volume_start () {
	gluster volume start ${VOLUME}
}

#volume_remove
volume_create
sleep ${SLEEP120}
volume_start
sleep ${SLEEP10}

#volume_start
[ -d ${MOUNTPOINT} ] || mkdir ${MOUNTPOINT}
mount -t glusterfs /etc/glusterd/vols/${VOLUME}/${VOLUME}-fuse.vol ${MOUNTPOINT}
