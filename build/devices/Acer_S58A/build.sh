#!/bin/bash
#########################################################################
#
# Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#
# Required Env Variable:
#     BRANCH_OUT_DIR  Absolute path for current branch on nas, start with
#                     /mnt/CloudDataSolution/TeraFonn_CI_build.
#
#     LIB_DIR         Absolute path for from previous jenkins job in pipeline.
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
set -e

#let printf handle the printing
function _hashes() { printf %0$((${1}))d\\n | tr 0 \# ; }

function _hdr_inc() { local _hinc=${1##*-} _hashc=${2##*[!0-9]}
	: ${_hinc:=$(set -- $3 ; printf %s_cnt\\n "${1-_hdr}")}
	${1+shift} ${2+2}
	_hashes ${_hashc:=40}
	printf "%s #$((${_hinc}=${_hinc}+1)):${1+%b}" \
		${1+"$*"} ${1+\\c}Default
	echo && _hashes $_hashc
}
function start_builder() {
	_hdr_inc - - Doing $FUNCNAME
	DOCKERNAME=s58a-ci-`date +%m%d-%H%M%S`
	docker run -d --name=$DOCKERNAME -v /zpool-docker/ccache:/root/.ccache docker:5000/android-buildbox:Acer_s58a_6.0_userdebug-v3
}
function stop_builder() {
	_hdr_inc - - Doing $FUNCNAME
	docker rm -f $DOCKERNAME
}
function setup_ssh_key() {
	_hdr_inc - - Doing $FUNCNAME
	if [ ! -f ~/.ssh/id_rsa.pub ]; then
		ssh-keygen -b 2048 -t rsa -f ~/.ssh/id_rsa -q -N ""
	fi
	mkdir -p $ZFS_PATH/root/.ssh
	cat ~/.ssh/id_rsa.pub >> $ZFS_PATH/root/.ssh/authorized_keys
	until docker top $DOCKERNAME | grep -q /usr/sbin/sshd
	do
		: "Wait builder sshd.."
		sleep 2
	done
}
function patch_system() {
	_hdr_inc - - Doing $FUNCNAME
	rsync -arcv --no-owner --no-times $repo/build/devices/Acer_S58A/patch/ $ANDROID_SRC_PATH/
}
function install_lib_to_out() {
	_hdr_inc - - Doing $FUNCNAME
	rsync -arcv --no-owner --no-times $LIB_DIR/acer-s58a-hcfs/system/ $ANDROID_OUT_PATH/system/
}
function install_apk_to_out() {
	_hdr_inc - - Doing $FUNCNAME
	mkdir -p $ANDROID_OUT_PATH/system/app/TeraFonn
	mkdir -p $ANDROID_OUT_PATH/system/lib/
	\cp -vf $APK_DIR/${APK_NAME}.apk $ANDROID_OUT_PATH/system/app/TeraFonn/TeraFonn.apk
	\cp -vf $APK_DIR/libterafonnapi.so $ANDROID_OUT_PATH/system/lib/
}
function publish_apk() {
	_hdr_inc - - Doing $FUNCNAME
	\cp -r $APK_DIR ${BRANCH_OUT_DIR}/
}
function build_system() {
	_hdr_inc - - Doing $FUNCNAME
	eval $DOCKER_SSH bash -ic "build"
}
function rebuild_system() {
	_hdr_inc - - Doing $FUNCNAME
	eval $DOCKER_SSH bash -ic "make snod"
}
function publish_image() {
	_hdr_inc - - Doing $FUNCNAME
	mkdir -p ${BRANCH_OUT_DIR}/${JOB_NAME}
	pushd $ZFS_PATH/root/acer_s58a/out/target/product/s58a/
	zip ${BRANCH_OUT_DIR}/${JOB_NAME}/images.zip *.img android-info.txt
	\cp -v boot.img system.img userdata.img ${BRANCH_OUT_DIR}/${JOB_NAME}
	popd
}
function publish_resource() {
	_hdr_inc - - Doing $FUNCNAME
	\cp -fv $here/resource/* ${BRANCH_OUT_DIR}/${JOB_NAME}
}
function mount_nas() {
	_hdr_inc - - Doing $FUNCNAME
	service rpcbind start || :
	if ! mount  | grep 'nas:/ubuntu on /mnt'; then
		umount /mnt || :
		mount nas:/ubuntu /mnt
	fi
}
function unmount_nas() {
	_hdr_inc - - Doing $FUNCNAME
	umount /mnt
}

start_builder

echo ========================================
echo Jenkins pass-through variables:
echo BRANCH_OUT_DIR: ${BRANCH_OUT_DIR}
echo LIB_DIR: ${LIB_DIR}
echo JOB_NAME: ${JOB_NAME}
echo ========================================
echo "Environment variables (with defaults):"
set -x
BRANCH_OUT_DIR=${BRANCH_OUT_DIR:-/mnt/CloudDataSolution/TeraFonn_CI_build/android-dev/2.0.3.ci.test}
LIB_DIR=${LIB_DIR:-/mnt/CloudDataSolution/TeraFonn_CI_build/android-dev/2.0.3.ci.test/HCFS-android-binary}
JOB_NAME=${JOB_NAME:-HCFS-s58a-image}
ZFS_PATH=/var/lib/docker/zfs/graph/`docker inspect --format "{{.Id}}" $DOCKERNAME`
ANDROID_SRC_PATH=$ZFS_PATH/root/acer_s58a
ANDROID_OUT_PATH=$ANDROID_SRC_PATH/out/target/product/s58a
APK_NAME=terafonn_1.0.0017
APK_DIR=/mnt/CloudDataSolution/HCFS_android/apk_release/$APK_NAME
DOCKER_SSH="ssh -o 'UserKnownHostsFile=/dev/null' -o 'StrictHostKeyChecking=no' root@`docker inspect --format "{{.NetworkSettings.IPAddress}}" $DOCKERNAME`"
set +x
echo ========================================

mount_nas
setup_ssh_key
patch_system
build_system
install_lib_to_out
install_apk_to_out
patch_system

build_system
publish_image
publish_resource
publish_apk
# unmount_nas keep nas mountpoint on ci server
stop_builder
