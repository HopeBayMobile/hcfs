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

echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
set -e -v -x
cd $here

function main()
{
	LOCK=$(tempfile)
	DEVICE=nexus-5x
	SRC_VERSION=6.0.0_r26_MDB08M_20160530

	if [ ! -d /data/android-6.0.0_r26/]; then
		mkdir -p /data/android-6.0.0_r26/
		pushd /data/android-6.0.0_r26/
		if ! hash repo; then
			curl https://storage.googleapis.com/git-repo-downloads/repo | sudo tee /bin/repo
			sudo chmod a+x /bin/repo
		fi
		repo init -u https://android.googlesource.com/platform/manifest -b android-6.0.0_r26
		repo sync -j32
		popd
	fi

	for FLAVOR in {source-only,prebuilt-user,prebuilt-userdebug}
	do
		IMG=docker:5000/${DEVICE}-buildbox:${FLAVOR}
		docker build -f Dockerfile.${FLAVOR} -t ${IMG} .
		if [ "${FLAVOR}" = "source-only" ]; then
			copy_system_source_code
		fi
		docker tag ${IMG} ${IMG}-${SRC_VERSION}
	done
	for FLAVOR in {source-only,prebuilt-user,prebuilt-userdebug}
	do
		IMG=docker:5000/${DEVICE}-buildbox:${FLAVOR}
		docker push ${IMG}
		docker push ${IMG}-${SRC_VERSION}
	done
}

function copy_system_source_code()
{
	DOCKERNAME=$(docker run -d docker:5000/${DEVICE}-buildbox:${FLAVOR})
	trap cleanup INT TERM
	setup_ssh_key
	rsync -arcv --no-owner --no-group --no-times \
		-e "ssh -o StrictHostKeyChecking=no" \
		--exclude=.repo \
		/data/android-6.0.0_r26/ \
		root@${DOCKER_IP}:/data/
	rsync -arcv --no-owner --no-group --no-times \
		-e "ssh -o StrictHostKeyChecking=no" \
		$repo/build/devices/Nexus_5X_MDB08M/patch/vendor/ \
		root@${DOCKER_IP}:/data/vendor/
	docker stop ${DOCKERNAME}
	docker commit ${DOCKERNAME} docker:5000/${DEVICE}-buildbox:${FLAVOR}
}

function cleanup() {
	trap "" INT TERM
	stop_builder
	exit
}
function stop_builder() {
	docker rm -f ${DOCKERNAME} || :
}
function check-ssh-agent() {
	[ -S "${SSH_AUTH_SOCK}" ] && { ssh-add -l >& /dev/null || [ $? -ne 2 ]; }
}
function setup_ssh_key() {
	if [ ! -f ~/.ssh/id_rsa.pub ]; then
		ssh-keygen -b 2048 -t rsa -f ~/.ssh/id_rsa -q -N ""
	fi
	cat ~/.ssh/id_rsa.pub | docker exec -i ${DOCKERNAME} \
		bash -c "cat >> /root/.ssh/authorized_keys; echo;cat /root/.ssh/authorized_keys"
	check-ssh-agent || export SSH_AUTH_SOCK=~/.tmp/ssh-agent.sock
	check-ssh-agent || eval "$(mkdir -p ~/.tmp && ssh-agent -s -a ~/.tmp/ssh-agent.sock)" > /dev/null
	ssh-add ~/.ssh/id_rsa
	until docker top ${DOCKERNAME} | grep -q /usr/sbin/sshd
	do
		: "Wait builder sshd.."
		sleep 2
	done
	DOCKER_IP=`docker inspect --format "{{.NetworkSettings.IPAddress}}" ${DOCKERNAME}`
	if [ -f $HOME/.ssh/known_hosts.old ]; then rm -f $HOME/.ssh/known_hosts.old; fi
	ssh-keygen -R ${DOCKER_IP}
	ssh -oBatchMode=yes -oStrictHostKeyChecking=no root@${DOCKER_IP} pwd
	TMP=$(tempfile)
	rsync $TMP root@${DOCKER_IP}:$TMP
}

main
