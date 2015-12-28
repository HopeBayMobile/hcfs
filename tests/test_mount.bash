#!/bin/bash
set -e
echo -e "\n======== ${BASH_SOURCE[0]} ========"
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "########## Setup Test Env"
. $WORKSPACE/utils/trace_error.bash
$WORKSPACE/utils/setup_dev_env.sh -m functional_test
. $WORKSPACE/utils/env_config.sh

if [[ $(id -Gn) != *fuse* ]]; then
	echo "########## Reload script with fuse group permission"
	exec sg fuse "${BASH_SOURCE[0]}"
fi
id
groups

cd $WORKSPACE/tests/functional_test/TestCases/HCFS

echo "########## Start local swift"
./setup_local_swift_env.bash

echo "########## Start hcfs deamon"
./start_hcfs.bash
. path_config.sh
sleep 1

echo "########## Create hcfs volume"
	set -x
while : ; do
	result=`$HCFSvol create autotest internal 2>&1` ||:
	echo -e "[$result]"
	if grep -q error <<< "$result"; then
		sleep 1
	elif grep -q Success <<< "$result"; then
		break
	fi
done

echo "########## Test Mount"
while : ; do
	result=`$HCFSvol mount autotest $WORKSPACE/tmp/mount 2>&1`
	echo -e "[$result]"
	(grep -q error <<< "$result") || break
	sleep 1
done
mount | grep "hcfs on $WORKSPACE/tmp/mount type fuse.hcfs"

echo "########## Test mount success"
