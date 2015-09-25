#!/bin/bash
echo ======== ${BASH_SOURCE[0]} ========
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../.. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
error() {
	local parent_lineno="$1"
	local message="$2"
	local code="${3:-1}"
	if [[ -n "$message" ]] ; then
		echo "Error on or near line ${parent_lineno}echo ${message}; exiting with status ${code}"
	else
		echo "Error on or near line ${parent_lineno}; exiting with status ${code}"
	fi
	exit "${code}"
}
trap 'error ${LINENO}' ERR

function cleanup {
	echo cleanup
	sudo umount $WORKSPACE/tmp/mount* || :
	sudo killall -9 hcfs || :
	sudo docker rm -f swift_test || :
	[ -d $WORKSPACE/tmp/ ] && -l $WORKSPACE/tmp/* || :
	sudo rm -rf $WORKSPACE/tmp/{meta,block,mount*,swift_data}
	mkdir -p $WORKSPACE/tmp/{meta,block,mount,swift_data}
}
#trap cleanup EXIT

set -e -x

# Setup Test Env
. $WORKSPACE/utils/setup_dev_env.sh -m functional_test

# Compile binary files
make -s -C $WORKSPACE/src/HCFS clean
make -s -C $WORKSPACE/src/CLI_utils clean
CFLAGS_ARG="`sed -rn -e '/^CFLAGS/s/ -Wall| -Wextra//g' \
	-e "/^CFLAGS/s/ *= *(.*)/='\1 -w'/p" $WORKSPACE/src/HCFS/Makefile`"
eval "make -s -C $WORKSPACE/src/HCFS $CFLAGS_ARG"

CFLAGS_ARG="`sed -rn -e '/^CFLAGS/s/ -Wall| -Wextra//g' \
	-e "/^CFLAGS/s/ *= *(.*)/='\1 -w'/p" $WORKSPACE/src/CLI_utils/Makefile`"
eval "make -s -C $WORKSPACE/src/CLI_utils $CFLAGS_ARG"

# Setup PATH for test
if ! grep -E "(^|:)$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils(:|$)" <<< `echo $PATH`; then
	export PATH="$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils:$PATH"
fi

echo "########## Test mount if there is docker service"
if [ -S /var/run/docker.sock ]; then

	cleanup

	echo "########## Start Swift server"
	SWIFT_ID=$(sudo docker run -d -t \
			-v $WORKSPACE/tmp/swift_data:/srv \
			-v /etc/localtime:/etc/localtime:ro \
			--name=swift_test aerofs/swift)

	echo "########## Get dynamic swift IP"
	SWIFT_IP=$(sudo docker inspect --format \
			'{{ .NetworkSettings.IPAddress }}' $SWIFT_ID)


	echo "########## Generate hcfs config file"
	sed -r -e "s@\%WORKSPACE\%@$WORKSPACE@g" -e "s@\%SWIFT_IP\%@$SWIFT_IP@g" \
		$here/TestCases/HCFS/hcfs_docker_swift.conf | sudo tee /etc/hcfs.conf

	echo "########## Wait swift ready"
	SWIFT="swift -A http://$SWIFT_IP:8080/auth/v1.0 -U test:tester -K testing"
	while ! $SWIFT stat; do sleep 1; done

	echo "########## Create Container"
	while $SWIFT post autotest_private_container |& grep failed; do sleep 1; done

	echo "########## Start hcfs deamon"
	hcfs &

	echo "########## Wait hcfs mount"
	while true; do
		result=`HCFSvol create autotest`
		echo $result | grep -q Success && break
		sleep 1
	done

	echo "########## Mount should succeed"
	HCFSvol mount autotest $WORKSPACE/tmp/mount
	mount | grep "hcfs on $WORKSPACE/tmp/mount type fuse.hcfs"

	echo "########## Unmount should succeed"
	HCFSvol unmount autotest
	! mount | grep "hcfs on $WORKSPACE/tmp/mount type fuse.hcfs"
	echo "########## delete should succeed"
	HCFSvol delete autotest
	! HCFSvol list | grep autotest
	echo "########## terminate should succeed"
	HCFSvol terminate
	echo "########## Wait hcfs to terminate"
	wait

	pushd "$here"
	python pi_tester.py -d debug -c HCFS_0
	popd

	cleanup
fi
