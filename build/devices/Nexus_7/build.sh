#!/bin/bash
#########################################################################
#
# Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#   Auto build nexus_7 image with jenkins server
# export VERSION_NUM=2.2.1.9999
# export PUBLISH_DIR=/mnt/nas/CloudDataSolution/TeraFonn_CI_build/0.0.0.ci.test
# export LIB_DIR=/mnt/nas/CloudDataSolution/TeraFonn_CI_build/2.2.1.0908-android-dev/HCFS-android-binary
# export APP_DIR=/mnt/nas/CloudDataSolution/TeraFonn_CI_build/2.2.1.0908-android-dev/HCFS-terafonn-apk
#
# Required Env Variable:
#   PUBLISH_DIR  Absolute path for current branch on nas, start with
#                     /mnt/CloudDataSolution/TeraFonn_CI_build.
#
#   LIB_DIR         Absolute path for from previous jenkins job in pipeline.
#
# Revision History
#   2016/12/29 Ripley nexus_7 build script based on nexus_5x
#
##########################################################################
[ $EUID -eq 0 ] || exec sudo -s -E $0 $@
echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
TRACE="set -xv"; UNTRACE="set +xv"
set +v -e -o functrace

function require_var (){
	eval printf '%-20s%s' $1 \${$1:-};echo
	if eval [ -z \"\${$1:-}\" ]; then
		echo Error: required environment variables $1 does not exist
		exit 1
	fi
}

DEVICE=Nexus_7
BOXNAME=nexus-7-buildbox
BINARY_TARGET=nexus-7-hcfs
HCFS_COMPOMENT_BASE=device/asus/flo

pwd
ls -la
echo ================================================================================
# Environment variables enjected by jenkins upstream jobs
require_var LIB_DIR
require_var APP_DIR
require_var PUBLISH_DIR
# local variables in this script
require_var DEVICE
require_var BOXNAME
require_var BINARY_TARGET
require_var HCFS_COMPOMENT_BASE
echo ================================================================================


function main()
{
	$UNTRACE
	IMAGE_TYPE=$1
	BRANCH_IN_7=${BRANCH_IN_7:-master}
	BRANCH_IN_LAUNCHER=${BRANCH_IN_LAUNCHER:-master}
	DEVICE_IMG=HCFS-nexus-7-image
	IMG_DIR=${PUBLISH_DIR}/${DEVICE_IMG}-${IMAGE_TYPE}
	#DOCKER_IMAGE="docker:5000/${BOXNAME}:prebuilt-${IMAGE_TYPE}-20160621-with-launcher"
	DOCKER_IMAGE="docker:5000/${BOXNAME}:source-only-6.0.0_r26_MDB08M_20161201"
	echo ================================================================================
	echo $IMAGE_TYPE
	echo $BRANCH_IN_7
	echo $IMG_DIR
	echo $DOCKER_IMAGE
	echo ================================================================================
	$TRACE

	mount_nas
	start_builder
	trap cleanup INT TERM ERR
	setup_ssh_key
	update_system_source
	pull_hcfs_binaay
	pull_management_app

	push_system_diff
	push_launcher_tag
	build_system
	publish_image
	stop_builder
}

#let printf handle the printing
function _hashes() { printf %0$((${1}))d\\n | tr 0 \# ; }

function _hdr_inc() {
	{ $UNTRACE; } 2>/dev/null
	local _hinc=${1##*-} _hashc=${2##*[!0-9]}
	: ${_hinc:=$(set -- $3 ; printf %s_cnt\\n "${1-_hdr}")}
	${1+shift} ${2+2}
	_hashes ${_hashc:=40}
	printf "%s #$((${_hinc}=${_hinc}+1)):${1+%b}" \
		${1+"$*"} ${1+\\c}Default
	echo && _hashes $_hashc
	$TRACE
}

function start_builder() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	DOCKERNAME="$BOXNAME-${IMAGE_TYPE}-${BUILD_NUMBER:-`date +%m%d-%H%M%S`}"
	eval docker pull $DOCKER_IMAGE || :
	echo ${DOCKERNAME} > DOCKERNAME # Leave container name for jenkins to cleanup
	eval docker run -v /data/ccache:/ccache -d --name=$DOCKERNAME $DOCKER_IMAGE
}

function cleanup() {
	trap "" INT TERM ERR
	stop_builder
}

function stop_builder() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME $1; } 2>/dev/null
	docker rm -f $DOCKERNAME || :
	rm -f DOCKERNAME
}

function check-ssh-agent() {
	[ -S "$SSH_AUTH_SOCK" ] && { ssh-add -l >& /dev/null || [ $? -ne 2 ]; }
}

function setup_ssh_key() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	if [ ! -f ~/.ssh/id_rsa.pub ]; then
		ssh-keygen -b 2048 -t rsa -f ~/.ssh/id_rsa -q -N "" || :
	fi
	cat ~/.ssh/id_rsa.pub | docker exec -i $DOCKERNAME \
		bash -c "cat >> /root/.ssh/authorized_keys; echo;cat /root/.ssh/authorized_keys"
	mkdir -p ~/.tmp
	check-ssh-agent || export SSH_AUTH_SOCK=~/.tmp/ssh-agent.sock
	check-ssh-agent || { rm -f ~/.tmp/ssh-agent.sock && eval "$(ssh-agent -s -a ~/.tmp/ssh-agent.sock)"; } > /dev/null
	ssh-add ~/.ssh/id_rsa
	DOCKER_IP=`docker inspect --format "{{.NetworkSettings.IPAddress}}" $DOCKERNAME`
	if [ -f $HOME/.ssh/known_hosts.old ]; then rm -f $HOME/.ssh/known_hosts.old; fi
	ssh-keygen -R $DOCKER_IP || :
	until ssh -oBatchMode=yes -oStrictHostKeyChecking=no root@$DOCKER_IP pwd
	do
		: "Wait builder sshd.."
		sleep 2
	done
	touch /tmp/test
	rsync /tmp/test root@$DOCKER_IP:/tmp/test
}

RSYNC_SETTING="-arcv --no-owner --no-group --no-times"
function update_system_source() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	ssh -t -o "BatchMode yes" root@$DOCKER_IP 'bash -il -c " \
	git pull origin '${BRANCH_IN_7}' && \
	git submodule update --init --recursive --remote && \
	git submodule foreach git pull origin '${BRANCH_FOR_SUBMODULE}'"'
}
function pull_hcfs_binaay() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync $RSYNC_SETTING $LIB_DIR/$BINARY_TARGET/system/ root@$DOCKER_IP:/data/$HCFS_COMPOMENT_BASE/hopebay/
}

function pull_management_app() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync $RSYNC_SETTING $APP_DIR/*.apk root@$DOCKER_IP:/data/$HCFS_COMPOMENT_BASE/HopebayHCFSmgmt/
	rsync $RSYNC_SETTING $APP_DIR/armeabi-v7a/ root@$DOCKER_IP:/data/$HCFS_COMPOMENT_BASE/hopebay/lib/
}

function push_system_diff() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	ssh -t -o "BatchMode yes" root@$DOCKER_IP 'bash -il -c " \
	git add -f * && \
	git commit -m '${VERSION_NUM}' && \
	git tag -a -m '${VERSION_NUM}' '${VERSION_NUM}' && \
	{ git push origin '${VERSION_NUM}' -f || :; }"'
}

function push_launcher_tag() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	ssh -t -o "BatchMode yes" root@$DOCKER_IP 'bash -il -c " \
	cd packages/apps/Launcher3/
	git tag -a -m '${VERSION_NUM}' '${VERSION_NUM}' && \
	{ git push origin '${VERSION_NUM}' -f || :; }"'
}

function build_system() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	ssh root@$DOCKER_IP ccache --max-size=30G
	ssh -t -o "BatchMode yes" root@$DOCKER_IP 'bash -il -c " \
	lunch aosp_bullhead-'${IMAGE_TYPE}' && \
	echo BUILD_NUMBER := '${BUILD_NUMBER:-}' >> build/core/build_id.mk && \
	echo DISPLAY_BUILD_NUMBER := true >> build/core/build_id.mk && \
	make \$PARALLEL_JOBS dist"'
}

function publish_image() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	if [ -d ${IMG_DIR} ]; then
		rm -rf ${IMG_DIR}
	fi

	CI_PATH=/mnt/nas/CloudDataSolution/TeraFonn_CI_build/release

	record_tmp=${PUBLISH_DIR}/recode.tmp
	if [ -e "$record_tmp" ]; then
		last_version_path=`cat ${record_tmp}`
	else
		last_version_path=`find ${CI_PATH} -maxdepth 1 -name \
			"*-OTA-*" | tail -2 | head -1`
	fi

	last_version=`echo $last_version_path | awk -F- {'print $3'}`
	last_target=`find ${last_version_path} \
		-path */${DEVICE_IMG}-${IMAGE_TYPE}/*-target_files-*.zip`

	mkdir -p ${IMG_DIR}
	rsync $RSYNC_SETTING -L $here/resource/* ${IMG_DIR}

	if [ "$gitlabSourceBranch" = "kitkat/dev" ] || \
		[[ "$gitlabSourceBranch" = *-OTA* ]]; then
		if [ -e "$last_target" ]; then
			rsync $RSYNC_SETTING ${last_target} root@$DOCKER_IP:/data/old.zip

			ssh -t -o "BatchMode yes" root@$DOCKER_IP 'bash -il -c \
			"/data/build/tools/releasetools/ota_from_target_files -i \
			/data/old.zip \
			/data/out/dist/*-target_files-* \
			/data/out/dist/'${VERSION_NUM}'-from-'${last_version}'.zip"'

			rsync $RSYNC_SETTING root@$DOCKER_IP:/data/out/dist/*-from-* ${IMG_DIR}
		fi
		rsync $RSYNC_SETTING root@$DOCKER_IP:/data/out/dist/{*-img-*,*-ota-*,*-target_files-*} ${IMG_DIR}
	else
		rsync $RSYNC_SETTING root@$DOCKER_IP:/data/out/dist/*-img-* ${IMG_DIR}
	fi
	pushd ${IMG_DIR}
	unzip *-img-*.zip
	popd

	if [ -e "$record_tmp" ]; then
		rm -f ${record_tmp}
	else
		echo $last_version_path > ${record_tmp}
	fi

	touch ${PUBLISH_DIR}
	touch ${IMG_DIR}
}

function mount_nas() {
	{ _hdr_inc - - Doing $FUNCNAME; } 2>/dev/null
	service rpcbind start || :
	if ! mount  | grep 'nas:/ubuntu on /mnt/nas'; then
		mkdir -p /mnt/nas
		mount nas:/ubuntu /mnt/nas || :
	fi
}


$TRACE
if [ -n "${1:-}" ]; then
	main $1
else
	main userdebug
	main user
fi
