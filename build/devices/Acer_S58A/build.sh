#!/bin/bash
##
## Copyright (c) 2021 HopeBayTech.
##
## This file is part of Tera.
## See https://github.com/HopeBayMobile for further info.
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
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
	if [ "$IMAGE_TYPE" = "push-branch" ];
	then
		DOCKER_IMAGE='docker:5000/s58a-buildbox:userdebug-prebuilt-v5.0419'
	else
		DOCKER_IMAGE='docker:5000/s58a-buildbox:${IMAGE_TYPE}-prebuilt-v5.0419'
	fi
	mkdir -p /data/ccache
	DOCKERNAME=s58a-build-${IMAGE_TYPE}-${BUILD_NUMBER:-`date +%m%d-%H%M%S`}
	eval docker pull $DOCKER_IMAGE || :
	eval docker run -d --name=$DOCKERNAME $DOCKER_IMAGE
	echo ${DOCKERNAME} > DOCKERNAME # Leave container name for jenkins to cleanup
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
function patch_system() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync -arcv --no-owner --no-group --no-times \
		$repo/build/devices/Acer_S58A/patch/ root@$DOCKER_IP:/data/
}
function copy_hcfs_to_source_tree() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync -arcv --no-owner --no-group --no-times \
		$LIB_DIR/acer-s58a-hcfs/system/ root@$DOCKER_IP:/data/device/acer/s58a/hopebay/
}
function copy_apk_to_source_tree() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	rsync -arcv --no-owner --no-group --no-times \
		$APP_DIR/*.apk root@$DOCKER_IP:/data/device/acer/common/apps/HopebayHCFSmgmt/
	rsync -arcv --no-owner --no-group --no-times \
		$APP_DIR/arm64-v8a/ root@$DOCKER_IP:/data/device/acer/s58a/hopebay/lib64/
}
function build_system() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	ssh root@$DOCKER_IP ccache --max-size=30G
	ssh root@$DOCKER_IP bash -ic ":; cd /data/;\
	echo BUILD_NUMBER := ${BUILD_NUMBER:-} >> build/core/build_id.mk;\
	echo DISPLAY_BUILD_NUMBER := true >> build/core/build_id.mk;\
	cat build/core/build_id.mk;\
	ENABLE_HCFS=1 ./build.sh -s s58a_aap_gen1 -v ${IMAGE_TYPE}"
}
function publish_image() {
	{ _hdr_inc - - BUILD_VARIANT $IMAGE_TYPE $FUNCNAME; } 2>/dev/null
	IMG_DIR=${PUBLISH_DIR}/HCFS-s58a-image-${IMAGE_TYPE}
	mkdir -p ${IMG_DIR}
	rsync -v $here/resource/* ${IMG_DIR}
	rsync -arcv --no-owner --no-group --no-times \
		root@$DOCKER_IP:/data/out/target/product/s58a/{boot.img,system.img,userdata.img} ${IMG_DIR}
	touch ${PUBLISH_DIR} ${IMG_DIR}
}
function mount_nas() {
	{ _hdr_inc - - Doing $FUNCNAME; } 2>/dev/null
	service rpcbind start || :
	if ! mount  | grep 'nas:/ubuntu on /mnt/nas'; then
		mkdir -p /mnt/nas
		mount nas:/ubuntu /mnt/nas || :
	fi
}
function make_s58a_source_patch() {
	rsync -arcv --no-owner --no-group --no-times \
		$here/README.txt root@$DOCKER_IP:/data/README.md
	ssh root@$DOCKER_IP bash -ic ": && cd /data && \
	git checkout -b tf/${VERSION_NUM} && \
	git add README.md \
	hb_patch/ \
	device/acer/common/apps/HopebayHCFSmgmt/ \
	device/acer/s58a/products/common/common.mk \
	device/acer/s58a/products/common/hb-common.mk \
	device/acer/s58a/hb_overlay/ \
	device/acer/s58a/hb_sepolicy/ \
	device/acer/s58a/hopebay/ \
	device/acer/s58a/init.target.rc; \
	git diff --staged --binary > Terafonn_${VERSION_NUM}.patch && \
	git commit -m \"TeraFonn ${VERSION_NUM}\" && \
	git push hb \"tf/${VERSION_NUM}\""

	rsync -arcv --no-owner --no-group --no-times \
		root@$DOCKER_IP:/data/Terafonn_${VERSION_NUM}.patch ./
	if [ -n "$PASSWORD" ]; then
		zip -P "$PASSWORD" -r Terafonn_${VERSION_NUM}.patch.zip Terafonn_${VERSION_NUM}.patch README.txt
		rsync -arcv --no-owner --no-group --no-times --remove-source-files \
			Terafonn_${VERSION_NUM}.patch.zip ${PUBLISH_DIR}/
	fi
	rm -f Terafonn_${VERSION_NUM}.patch
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
eval '[ -n "${PUBLISH_DIR}" ]' || { echo Error: required parameter PUBLISH_DIR does not exist; exit 1; }

echo ========================================
echo Jenkins pass-through variables:
echo LIB_DIR: ${LIB_DIR}
echo PUBLISH_DIR: ${PUBLISH_DIR}
echo ========================================
echo "Environment variables (with defaults):"
$TRACE

mount_nas

if [ "$MAKE_HCFS_PATCH" = 1 ]; then
	IMAGE_TYPE="push-branch"
	start_builder
	setup_ssh_key
	patch_system
	copy_hcfs_to_source_tree
	copy_apk_to_source_tree
	make_s58a_source_patch
	stop_builder
	exit
fi

if [ -n "$IMAGE_TYPE" ]; then
	build_image_type "$IMAGE_TYPE"
else
	for IMAGE_TYPE in userdebug user
	do
		build_image_type "$IMAGE_TYPE"
	done
fi
