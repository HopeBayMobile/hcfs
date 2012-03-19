SHELL=/bin/bash
#DATE=`date +%Y%m%d%H%M%S`
#MANAGEMENT_HOST="10.7.4.4"
#MANAGEMENT_HOST="10.7.6.3"
MANAGEMENT_HOST="140.112.181.136"
SUBNET1="10.7.0.0/24"
SUBNET2="10.7.0.0/24"
SUBNET3="10.7.0.0/25"
PROJECT_NAME=storageappliance

all: start

#start: reset mgnt dist
start: mgnt dist
	@scp ${PROJECT_NAME}.tgz nii@${MANAGEMENT_HOST}:/home/nii
	@ssh nii@${MANAGEMENT_HOST} "tar zxvf ${PROJECT_NAME}.tgz"
	#@ssh nii@${MANAGEMENT_HOST}
	#@ssh root@${MANAGEMENT_HOST} "./${PROJECT_NAME}/run.sh ${SUBNET1} ${SUBNET2} ${SUBNET3}"

mgnt:
	./sshcopyid.sh ${MANAGEMENT_HOST}

reset:
	@cd ./scripts && ./resetAll.sh ${SUBNET1}
	sleep 10
	@cd ./scripts && ./resetAll.sh ${SUBNET2}
	sleep 10
	@cd ./scripts && ./resetAll.sh ${SUBNET3}
	sleep 10
	#@cd ./scripts && ./allReset.sh

dist:
	[ -d ${PROJECT_NAME} ] || mkdir -p ${PROJECT_NAME}
	@cp -r scripts \
	*.sh \
	*.expect \
	Makefile ${PROJECT_NAME}
	#@tar zcvf ${PROJECT_NAME}-${DATE}.tgz ${PROJECT_NAME}
	@tar zcvf ${PROJECT_NAME}.tgz ${PROJECT_NAME}

clean:
	@rm -rf ${PROJECT_NAME} *.tgz *~
