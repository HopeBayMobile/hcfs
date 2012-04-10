#!/bin/bash

SUBNET1=$1
SUBNET2=$2
SUBNET3=$3
ENABLE_SUBNET1=0
ENABLE_SUBNET2=0
ENABLE_SUBNET3=0
EXPORT1_BRICK1="/export1/brick1"
EXPORT2_BRICK1="/export2/brick1"


if [ ! -z ${SUBNET1} ]; then
	ENABLE_SUBNET1=1
else
	echo "Please provide prefix/subnet"
	exit 1
fi
[[ ! -z ${SUBNET2} ]] && ENABLE_SUBNET2=1
[[ ! -z ${SUBNET3} ]] && ENABLE_SUBNET3=1

#./scripts/resetAll.sh ${SUBNET1}
#./scripts/resetAll.sh ${SUBNET2}
#./scripts/resetAll.sh ${SUBNET3}

GLUSTERDLOG="./glusterd.log"
[[ -f ${GLUSTERDLOG} ]] && rm ${GLUSTERDLOG}

PORT=24007
SLEEP120=120
SLEEP10=10
SLEEP1=1
VOLUME="ntu-management"
MOUNTPOINT="/mnt/${VOLUME}"
#HOSTUpperBound=200
HOSTUpperBound=80
j=1

if [ ! -z ${ENABLE_SUBNET1} ]; then
SUBNET1_NETWORK_PREFIX=`echo ${SUBNET1}| cut -d "/" -f 1`

SUBNET1_NETWORK_PREFIX1=`echo ${SUBNET1_NETWORK_PREFIX}| cut -d "." -f 1`
SUBNET1_NETWORK_PREFIX2=`echo ${SUBNET1_NETWORK_PREFIX}| cut -d "." -f 2`
SUBNET1_NETWORK_PREFIX3=`echo ${SUBNET1_NETWORK_PREFIX}| cut -d "." -f 3`

SUBNET1_NETWORK=${SUBNET1_NETWORK_PREFIX1}.${SUBNET1_NETWORK_PREFIX2}.${SUBNET1_NETWORK_PREFIX3}

SUBNET1_CIDR=`echo ${SUBNET1}| cut -d "/" -f 2`

case ${SUBNET1_CIDR} in
24)
	SUBNET1_NUMBERofIPADDRESS=254
	;;
25)
	SUBNET1_NUMBERofIPADDRESS=126
	;;
26)
	SUBNET1_NUMBERofIPADDRESS=62
	;;
*)
    echo "Usage: ./createall.sh 10.7.0.0/24 10.7.0.0/24 10.7.0.0/25 CIDR ${SUBNET3_CIDR} is not supported"
	exit 1
	;;
esac

for i in `seq 1 ${SUBNET1_NUMBERofIPADDRESS}`; do

        ./telexpectcheck.expect ${SUBNET1_NETWORK}.${i} $PORT &>> ${GLUSTERDLOG}

done

fi


if [ ! -z ${ENABLE_SUBNET2} ]; then
SUBNET2_NETWORK_PREFIX=`echo ${SUBNET2}| cut -d "/" -f 1`

SUBNET2_NETWORK_PREFIX1=`echo ${SUBNET2_NETWORK_PREFIX}| cut -d "." -f 1`
SUBNET2_NETWORK_PREFIX2=`echo ${SUBNET2_NETWORK_PREFIX}| cut -d "." -f 2`
SUBNET2_NETWORK_PREFIX3=`echo ${SUBNET2_NETWORK_PREFIX}| cut -d "." -f 3`

SUBNET2_NETWORK=${SUBNET2_NETWORK_PREFIX1}.${SUBNET2_NETWORK_PREFIX2}.${SUBNET2_NETWORK_PREFIX3}
SUBNET2_CIDR=`echo ${SUBNET2}| cut -d "/" -f 2`

case ${SUBNET2_CIDR} in
24)
	SUBNET2_NUMBERofIPADDRESS=254
	;;
25)
	SUBNET2_NUMBERofIPADDRESS=126
	;;
26)
	SUBNET2_NUMBERofIPADDRESS=62
	;;
*)
    echo "No 2nd subnet"
	;;
esac

for i in `seq 1 ${SUBNET2_NUMBERofIPADDRESS}`; do

        ./telexpectcheck.expect ${SUBNET2_NETWORK}.${i} $PORT &>> ${GLUSTERDLOG}

done

fi

if [ ! -z ${ENABLE_SUBNET3} ]; then
SUBNET3_NETWORK_PREFIX=`echo ${SUBNET3}| cut -d "/" -f 1`

SUBNET3_NETWORK_PREFIX1=`echo ${SUBNET3_NETWORK_PREFIX}| cut -d "." -f 1`
SUBNET3_NETWORK_PREFIX2=`echo ${SUBNET3_NETWORK_PREFIX}| cut -d "." -f 2`
SUBNET3_NETWORK_PREFIX3=`echo ${SUBNET3_NETWORK_PREFIX}| cut -d "." -f 3`

SUBNET3_NETWORK=${SUBNET3_NETWORK_PREFIX1}.${SUBNET3_NETWORK_PREFIX2}.${SUBNET3_NETWORK_PREFIX3}

SUBNET3_CIDR=`echo ${SUBNET3}| cut -d "/" -f 2`

case ${SUBNET3_CIDR} in
24)
	SUBNET3_NUMBERofIPADDRESS=254
	;;
25)
	SUBNET3_NUMBERofIPADDRESS=126
	;;
26)
	SUBNET3_NUMBERofIPADDRESS=62
	;;
*)
    echo "No third subnet"
	;;

esac

for i in `seq 1 ${SUBNET3_NUMBERofIPADDRESS}`; do

        ./telexpectcheck.expect ${SUBNET3_NETWORK}.${i} $PORT &>> ${GLUSTERDLOG}

done
fi

#echo "${SUBNET1_NETWORK} ${SUBNET2_NETWORK} ${SUBNET3_NETWORK}"
#echo "${SUBNET1_CIDR} ${SUBNET2_CIDR} ${SUBNET3_CIDR}"



GLUSTERD_ALIVE="glusterd is alive"
ALLIP_LIST=`cat ${GLUSTERDLOG} | grep "glusterd is alive"  | awk ' { print $1 }' `

echo ${ALLIP_LIST} > iplist



for i in ${ALLIP_LIST}; do
	if [ ${j} -le ${HOSTUpperBound} ]; then
	DATE=`date +%Y%m%d%H%M%S`
	NEWIP_LIST="${NEWIP_LIST} ${i}"
	echo "Peer ${j}: ${DATE} probing ip ${i}"
		if [ ${j} == ${HOSTUpperBound} ]; then
			echo "stop at number of peer: ${HOSTUpperBound}"
		#exit 0
		fi
	gluster peer probe ${i}
	sleep ${SLEEP1}
	gluster peer status
	let j=j+1
	fi
done


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
	#BRICK_LIST="${BRICK_LIST} ${i}:/mnt/disk1 ${i}:/mnt/disk2"
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
