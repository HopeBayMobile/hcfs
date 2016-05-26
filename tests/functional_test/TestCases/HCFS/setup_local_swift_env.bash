#!/bin/bash
#########################################################################
#
# Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#
# Revision History
#   2016/1/18 Jethro unified usage of workspace path
#
##########################################################################

echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $repo

exec 1> >(while read line; do echo -e "        $line"; done;)
exec 2> >(while read line; do echo -e "        $line" >&2; done;)

function cleanup {
	echo "########## Cleanup"
	sudo docker rm -f swift_test || :
	mkdir -p $repo/tmp/{swift_data,meta,block}
	sudo find $repo/tmp/{swift_data,meta,block} -mindepth 1 -delete
}

echo "########## Setup Test Env"
$repo/utils/setup_dev_env.sh -m functional_test
$repo/utils/setup_dev_env.sh -m docker_host
. $repo/utils/env_config.sh

echo "########## Test mount if there is docker service"
if [ ! -S /var/run/docker.sock ]; then
	echo Error: need docker to start local swift service
	exit 1
fi

cleanup

echo "########## Start Swift server"
if [ -f /.dockerinit ]; then
	HOST_WORKSPACE="$(sudo docker inspect hcfs_test | grep -e /home/jenkins/repo/HCFS | tr ':"' $'\n' | sed -n "2p")"
else
	HOST_WORKSPACE="$repo"
fi
echo "HOST_WORKSPACE = $HOST_WORKSPACE"

SWIFT_ID=$(sudo docker run -d -t \
		-v $HOST_WORKSPACE/tmp/swift_data:/srv \
		-v /etc/localtime:/etc/localtime:ro \
		--name=swift_test aerofs/swift)

echo "########## Get dynamic swift IP"
SWIFT_IP=$(sudo docker inspect --format '{{ .NetworkSettings.IPAddress }}' $SWIFT_ID)


echo "########## Generate hcfs config file"
sudo mkdir -p /data
sed -r -e "s@\%repo\%@$repo@g" -e "s@\%SWIFT_IP\%@$SWIFT_IP@g" -e "s@\%WORKSPACE\%@$repo@g" \
	$here/hcfs_docker_swift.conf | sudo tee /etc/hcfs.conf | sudo tee /data/hcfs.conf.plain

echo "########## Wait swift ready"
# Fix proxy error
unset http_proxy
SWIFT="swift -A http://$SWIFT_IP:8080/auth/v1.0 -U test:tester -K testing"
while :; do
	out=`$SWIFT stat 2>&1`
	code=$?
	echo -e "$out"
	if grep -q failed <<<"$out"; then
		sleep 1
		continue
	fi
	break
done

echo "########## Create Container"
while :; do
	$SWIFT post autotest_private_container
	if [[ $? -ne 0 ]]; then
		sleep 1
		continue
	fi
	out=`$SWIFT stat autotest_private_container 2>&1`
	echo -e "$out"
	if ! grep -q Container <<<"$out"; then
		sleep 1
		continue
	fi
	break
done
