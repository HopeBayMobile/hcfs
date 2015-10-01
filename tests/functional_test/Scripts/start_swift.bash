#!/bin/bash
echo -e "\n======== ${BASH_SOURCE[0]} ========"
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../../.. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function cleanup {
	echo "########## Cleanup"
	sudo docker rm -f swift_test || :
	sudo rm -rf $WORKSPACE/tmp/swift_data
	mkdir -p $WORKSPACE/tmp/swift_data
}


echo "########## Setup Test Env"
. $WORKSPACE/utils/trace_error.bash
set -e
$WORKSPACE/utils/setup_dev_env.sh -m functional_test
$WORKSPACE/utils/setup_dev_env.sh -m docker_host
. $WORKSPACE/utils/env_config.sh

echo "########## Test mount if there is docker service"
if [ ! -S /var/run/docker.sock ]; then
	echo Error: need docker to start local swift service
	exit 1
fi

cleanup

echo "########## Start Swift server"
SWIFT_ID=$(sudo docker run -d -t \
		-v $WORKSPACE/tmp/swift_data:/srv \
		-v /etc/localtime:/etc/localtime:ro \
		--name=swift_test aerofs/swift)

echo "########## Get dynamic swift IP"
SWIFT_IP=$(sudo docker inspect --format '{{ .NetworkSettings.IPAddress }}' $SWIFT_ID)


echo "########## Generate hcfs config file"
sed -r -e "s@\%WORKSPACE\%@$WORKSPACE@g" -e "s@\%SWIFT_IP\%@$SWIFT_IP@g" \
	$here/hcfs_docker_swift.conf | sudo tee /etc/hcfs.conf

echo "########## Wait swift ready"
SWIFT="swift -A http://$SWIFT_IP:8080/auth/v1.0 -U test:tester -K testing"
while :; do
	out=`$SWIFT stat`
	code=$?
	echo -e "$out"
	if [ $code -eq 0 ]; then
		break
	fi
	sleep 1
done

echo "########## Create Container"
while :; do
	$SWIFT post autotest_private_container
	out=`$SWIFT stat autotest_private_container`
	echo -e "$out"
	if grep -q Container <<<"$out"; then
		break
	fi
	sleep 1
done
