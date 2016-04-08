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
#     LIB_DIR         Absolute path for from previous jenkins job in pipeline.
#
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
	DOCKERNAME=s58a-build-${IMAGE_TYPE}-${BUILD_NUMBER:-`date +%m%d-%H%M%S`}
	eval docker pull $DOCKER_IMAGE || :
	eval docker run -d --name=$DOCKERNAME -v /data/ccache:/root/.ccache $DOCKER_IMAGE
	trap cleanup INT TERM
}
function cleanup() {
	trap "" INT TERM
	stop_builder
	exit
}
function stop_builder() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME $1; } 2>/dev/null
	docker rm -f $DOCKERNAME || :
}
function setup_ssh_key() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	if [ ! -f ~/.ssh/id_rsa.pub ]; then
		ssh-keygen -b 2048 -t rsa -f ~/.ssh/id_rsa -q -N ""
	fi
	cat ~/.ssh/id_rsa.pub | docker exec -i $DOCKERNAME \
		bash -c "cat >> /root/.ssh/authorized_keys; echo;cat /root/.ssh/authorized_keys"
	eval $(ssh-agent)
	ssh-add ~/.ssh/id_rsa
	until docker top $DOCKERNAME | grep -q /usr/sbin/sshd
	do
		: "Wait builder sshd.."
		sleep 2
	done
	DOCKER_IP=`docker inspect --format "{{.NetworkSettings.IPAddress}}" $DOCKERNAME`
	ssh-keygen -R $DOCKER_IP
	ssh -o StrictHostKeyChecking=no root@$DOCKER_IP pwd
}
function patch_system() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync -arcv --no-owner --no-group --no-times -e "ssh -o StrictHostKeyChecking=no" \
		$repo/build/devices/Acer_S58A/patch/ root@$DOCKER_IP:/data/
}
function copy_hcfs_to_source_tree() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync -arcv --no-owner --no-group --no-times -e "ssh -o StrictHostKeyChecking=no" \
		$LIB_DIR/acer-s58a-hcfs/system/ root@$DOCKER_IP:/data/device/acer/s58a/hopebay/
}
function copy_apk_to_source_tree() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync -arcv --no-owner --no-group --no-times -e "ssh -o StrictHostKeyChecking=no" \
		$APP_DIR/*.apk root@$DOCKER_IP:/data/device/acer/common/apps/HopebayHCFSmgmt/
	rsync -arcv --no-owner --no-group --no-times -e "ssh -o StrictHostKeyChecking=no" \
		$APP_DIR/arm64-v8a/ root@$DOCKER_IP:/data/device/acer/s58a/hopebay/lib64/
}
function build_system() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	ssh -o StrictHostKeyChecking=no root@$DOCKER_IP ccache --max-size=30G
	ssh -o StrictHostKeyChecking=no root@$DOCKER_IP bash -ic ":; cd /data/;\
	echo BUILD_NUMBER := ${BUILD_NUMBER:-} >> build/core/build_id.mk;\
	echo DISPLAY_BUILD_NUMBER := true >> build/core/build_id.mk;\
	cat build/core/build_id.mk;\
	./build.sh -s s58a_aap_gen1 -v ${IMAGE_TYPE}"
}
function publish_image() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	IMG_DIR=${PUBLISH_DIR}/HCFS-s58a-image-${IMAGE_TYPE}
	mkdir -p $IMG_DIR
	rsync -v $here/resource/* $IMG_DIR
	rsync -v root@$DOCKER_IP:/data/out/target/product/s58a/{boot.img,system.img,userdata.img} $IMG_DIR
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
function build_image_type() {
	{ _hdr_inc - - Doing $FUNCNAME; } 2>/dev/null
	IMAGE_TYPE="$1"
	start_builder
	setup_ssh_key
	patch_system
	copy_hcfs_to_source_tree
	copy_apk_to_source_tree
	build_system
	publish_image
	stop_builder
}

# LIB_DIR=${LIB_DIR:-/mnt/nas/CloudDataSolution/TeraFonn_CI_build/2.0.5.0391-android-dev/HCFS-android-binary}
# APP_DIR=${APP_DIR:-/mnt/nas/CloudDataSolution/TeraFonn_CI_build/2.0.5.0391-android-dev/HCFS-terafonn-apk}
# PUBLISH_DIR=${PUBLISH_DIR:-/mnt/nas/CloudDataSolution/TeraFonn_CI_build/0.0.0.ci.test}
eval '[ -n "$LIB_DIR" ]' || { echo Error: required parameter LIB_DIR does not exist; exit 1; }
eval '[ -n "$APP_DIR" ]' || { echo Error: required parameter APP_DIR does not exist; exit 1; }
eval '[ -n "$PUBLISH_DIR" ]' || { echo Error: required parameter PUBLISH_DIR does not exist; exit 1; }
DOCKER_IMAGE='docker:5000/s58a-buildbox:v4.0323-${IMAGE_TYPE}-prebuilt'

echo ========================================
echo Jenkins pass-through variables:
echo LIB_DIR: ${LIB_DIR}
echo PUBLISH_DIR: ${PUBLISH_DIR}
echo ========================================
echo "Environment variables (with defaults):"
$TRACE

mount_nas

if [ -n "$IMAGE_TYPE" ]; then
	build_image_type "$IMAGE_TYPE"
else
	for IMAGE_TYPE in user userdebug
	do
		build_image_type "$IMAGE_TYPE"
	done
fi
