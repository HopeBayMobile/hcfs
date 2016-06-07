#!/bin/bash
#########################################################################
#
# Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#   Auto build nexus_5x image with jenkins server
#
# Required Env Variable:
#   PUBLISH_DIR  Absolute path for current branch on nas, start with
#                     /mnt/CloudDataSolution/TeraFonn_CI_build.
#
#   LIB_DIR         Absolute path for from previous jenkins job in pipeline.
#
# Revision History
#   2016/5/30 Jethro nexus_5x build script based on s58a
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
	if eval '[ -n "$'$1'" ]'; then
		echo -e $1:\t$1
	else
		echo Error: required environment variables $1 does not exist
		exit 1
	fi
}

DEVICE=Nexus_5X_MDB08M
BOXNAME=nexus-5x-buildbox
BINARY_TARGET=nexus-5x-hcfs
HCFS_COMPOMENT_BASE=device/lge/bullhead

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
exit


function main()
{
	$UNTRACE
	IMAGE_TYPE=$1
	IMG_DIR=${PUBLISH_DIR}/HCFS-nexus-5x-image-${IMAGE_TYPE}
	DOCKER_IMAGE="docker:5000/${BOXNAME}:prebuilt-${IMAGE_TYPE}-6.0.0_r26_MDB08M_20160530"
	echo ================================================================================
	require_var IMAGE_TYPE
	require_var IMG_DIR
	require_var DOCKER_IMAGE
	echo ================================================================================
	$TRACE

	mount_nas
	start_builder
	trap cleanup INT TERM ERR
	setup_ssh_key
	pull_hcfs_binaay
	pull_management_app
	patch_system
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
	eval docker run -d --name=$DOCKERNAME $DOCKER_IMAGE
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
		ssh-keygen -b 2048 -t rsa -f ~/.ssh/id_rsa -q -N ""
	fi
	cat ~/.ssh/id_rsa.pub | docker exec -i $DOCKERNAME \
		bash -c "cat >> /root/.ssh/authorized_keys; echo;cat /root/.ssh/authorized_keys"
	check-ssh-agent || export SSH_AUTH_SOCK=~/.tmp/ssh-agent.sock
	check-ssh-agent || eval "$(mkdir -p ~/.tmp && ssh-agent -s -a ~/.tmp/ssh-agent.sock)" > /dev/null
	ssh-add ~/.ssh/id_rsa
	until docker top $DOCKERNAME | grep -q /usr/sbin/sshd
	do
		: "Wait builder sshd.."
		sleep 2
	done
	DOCKER_IP=`docker inspect --format "{{.NetworkSettings.IPAddress}}" $DOCKERNAME`
	if [ -f $HOME/.ssh/known_hosts.old ]; then rm -f $HOME/.ssh/known_hosts.old; fi
	ssh-keygen -R $DOCKER_IP
	ssh -oBatchMode=yes -oStrictHostKeyChecking=no root@$DOCKER_IP pwd
	touch /tmp/test
	rsync /tmp/test root@$DOCKER_IP:/tmp/test
}

RSYNC_SETTING="-arcv --no-owner --no-group --no-times"
function patch_system() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync $RSYNC_SETTING $here/patch/ \
		root@$DOCKER_IP:/data/
}
function pull_hcfs_binaay() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync $RSYNC_SETTING $LIB_DIR/$BINARY_TARGET/system/ \
		root@$DOCKER_IP:/data/$HCFS_COMPOMENT_BASE/hopebay/
}

function pull_management_app() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync $RSYNC_SETTING $APP_DIR/*.apk \
		root@$DOCKER_IP:/data/$HCFS_COMPOMENT_BASE/HopebayHCFSmgmt/
	rsync $RSYNC_SETTING $APP_DIR/arm64-v8a/ \
		root@$DOCKER_IP:/data/$HCFS_COMPOMENT_BASE/hopebay/lib64/
}

function build_system() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	ssh root@$DOCKER_IP ccache --max-size=30G
	ssh -t -o "BatchMode yes" root@$DOCKER_IP 'bash -il -c " \
	echo BUILD_NUMBER := '${BUILD_NUMBER:-}' >> build/core/build_id.mk && \
	echo DISPLAY_BUILD_NUMBER := true >> build/core/build_id.mk && \
	lunch aosp_bullhead-'${IMAGE_TYPE}' && \
	make \$PARALLEL_JOBS"'
}

function publish_image() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	mkdir -p $IMG_DIR
	rsync $RSYNC_SETTING --delete \
		$here/resource/* \
		$IMG_DIR
	rsync $RSYNC_SETTING \
		root@$DOCKER_IP:/data/out/target/product/*/{boot,system,userdata,vendor,recovery,cache}.img \
		$IMG_DIR
}

function mount_nas() {
	{ _hdr_inc - - Doing $FUNCNAME; } 2>/dev/null
	service rpcbind start || :
	if ! mount  | grep 'nas:/ubuntu on /mnt/nas'; then
		umount /mnt/nas || :
		mount nas:/ubuntu /mnt/nas
	fi
}

function unmount_nas() {
	{ _hdr_inc - - Doing $FUNCNAME; } 2>/dev/null
	umount /mnt/nas
}


$TRACE
if [ -n "$1" ]; then
	main $1
else
	main userdebug
	main user
fi
