#!/bin/bash
#########################################################################
#
# Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#
# Required Env Variable:
#     PUBLISH_DIR  Absolute path for current branch on nas, start with
#                     /mnt/CloudDataSolution/TeraFonn_CI_build.
#
#     UPSTREAM_LIB_DIR         Absolute path for from previous jenkins job in pipeline.
#
#     JOB_NAME        image build job name given by jenkins.
# Revision History
#   2016/2/8 Jethro Add ci script for s58a image
#
##########################################################################
[ $EUID -eq 0 ] || exec sudo -s -E $0
echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash

pwd;ls -la
set -e -o functrace
TRACE="set -x"; UNTRACE="set +x"

echo ========================================
echo Jenkins pass-through variables:
echo UPSTREAM_LIB_DIR: ${UPSTREAM_LIB_DIR}
echo PUBLISH_DIR: ${PUBLISH_DIR}
echo JOB_NAME: ${JOB_NAME}
echo ========================================
echo "Environment variables (with defaults):"
$TRACE

### Upstream hcfs lib
# UPSTREAM_LIB_DIR=${UPSTREAM_LIB_DIR:-/mnt/nas/CloudDataSolution/TeraFonn_CI_build/device/s58a_ci/2.0.3.0261/HCFS-android-binary}
eval '[ -n "$UPSTREAM_LIB_DIR" ]' || { echo Assign these for local build; exit 1; }

### Upstream APK
APP_NAME=${APP_NAME:-terafonn_1.0.0026}
APP_DIR=${APP_DIR:-/mnt/nas/CloudDataSolution/HCFS_android/apk_release/$APP_NAME}
DOCKER_IMAGE='docker:5000/s58a-buildbox:v4.0323-${IMAGE_TYPE}-prebuilt'

### Publish dir
# PUBLISH_DIR=${PUBLISH_DIR:-/mnt/nas/CloudDataSolution/TeraFonn_CI_build/android-dev/2.0.3.ci.test}
eval '[ -n "$PUBLISH_DIR" ]' || { echo Assign these for local build; exit 1; }
JOB_NAME=${JOB_NAME:-HCFS-s58a-image}

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
	mkdir -p /data/ccache
	DOCKERNAME=s58a-build-${IMAGE_TYPE}-`date +%m%d-%H%M%S`
	eval docker pull $DOCKER_IMAGE || :
	eval docker run -d --name=$DOCKERNAME -v /data/ccache:/root/.ccache $DOCKER_IMAGE
}
function stop_builder() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME $1; } 2>/dev/null
	docker rm -f $DOCKERNAME || :
}

alias ssh="ssh -o 'UserKnownHostsFile=/dev/null' -o 'StrictHostKeyChecking=no'"
function setup_ssh_key() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	if [ ! -f ~/.ssh/id_rsa.pub ]; then
		ssh-keygen -b 2048 -t rsa -f ~/.ssh/id_rsa -q -N ""
	fi
	cat ~/.ssh/id_rsa.pub | docker exec -i $DOCKERNAME bash -c "cat >> /root/.ssh/authorized_keys; echo;cat /root/.ssh/authorized_keys"
	eval $(ssh-agent)
	ssh-add ~/.ssh/id_rsa
	until docker top $DOCKERNAME | grep -q /usr/sbin/sshd
	do
		: "Wait builder sshd.."
		sleep 2
	done
	DOCKER_IP=`docker inspect --format "{{.NetworkSettings.IPAddress}}" $DOCKERNAME`
	DOCKER_SSH="ssh -o 'UserKnownHostsFile=/dev/null' -o 'StrictHostKeyChecking=no' root@$DOCKER_IP"
	ssh -o 'UserKnownHostsFile=/dev/null' -o 'StrictHostKeyChecking=no' root@$DOCKER_IP pwd
}
function patch_system() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync -arcv --no-owner --no-group --no-times -e "ssh -o StrictHostKeyChecking=no" \
		$repo/build/devices/Acer_S58A/patch/ root@$DOCKER_IP:/data/
}
function copy_hcfs_to_source_tree() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync -arcv --no-owner --no-group --no-times -e "ssh -o StrictHostKeyChecking=no" \
		$UPSTREAM_LIB_DIR/acer-s58a-hcfs/system/ root@$DOCKER_IP:/data/device/acer/s58a/hopebay/
}
function copy_apk_to_source_tree() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync -arcv --no-owner --no-group --no-times -e "ssh -o StrictHostKeyChecking=no" \
		$APP_DIR/${APP_NAME}.apk root@$DOCKER_IP:/data/device/acer/common/apps/HopebayHCFSmgmt/
	rsync -arcv --no-owner --no-group --no-times -e "ssh -o StrictHostKeyChecking=no" \
		$APP_DIR/arm64-v8a/ root@$DOCKER_IP:/data/device/acer/s58a/hopebay/lib64/
}
function build_system() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	ssh root@$DOCKER_IP ccache --max-size=30G
	ssh root@$DOCKER_IP cd /data/external/sepolicy \&\& \
		tools/post_process_mac_perms -s terafonn -d /data/device/acer/common/apps/HopebayHCFSmgmt -f mac_permissions.xml \&\& \
		cat mac_permissions.xml
	ssh root@$DOCKER_IP bash -ic "./build.sh -s s58a_aap_gen1 -v ${IMAGE_TYPE}"
}
function publish_image() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync -v \
		$here/resource/* \
		${PUBLISH_DIR}/${JOB_NAME}-${IMAGE_TYPE}
	rsync -v \
		root@$DOCKER_IP:/data/out/target/product/s58a/{boot.img,system.img,userdata.img} \
		${PUBLISH_DIR}/${JOB_NAME}-${IMAGE_TYPE}
	touch ${PUBLISH_DIR}/${JOB_NAME}-${IMAGE_TYPE}/use_${APP_NAME}
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
trap stop_builder EXIT # Cleanup docker container

mount_nas

for IMAGE_TYPE in user userdebug
do
	start_builder
	setup_ssh_key
	patch_system
	copy_hcfs_to_source_tree
	copy_apk_to_source_tree
	build_system
	publish_image
	stop_builder
done
