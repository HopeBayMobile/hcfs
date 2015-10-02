#!/bin/bash
set -e
echo -e "\n======== ${BASH_SOURCE[0]} ========"
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../../../.. && pwd )"
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

echo "########## Compile binary files"
$here/compile_hcfs_bin.bash
. $here/path_config.sh

echo "########## Start local swift"
$here/setup_local_swift_env.bash

echo "########## Start hcfs deamon"
$here/start_hcfs.bash
sleep 1

echo "########## Create hcfs volume"
while : ; do
	result=`$HCFSvol create autotest 2>&1`
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

echo "########## Test root access"
sudo touch $WORKSPACE/tmp/mount/test`date +%s`
ls -l $WORKSPACE/tmp/mount
sudo rm -rvf $WORKSPACE/tmp/mount/*

echo "########## Test unmount"
$HCFSvol unmount autotest
! mount | grep "hcfs on $WORKSPACE/tmp/mount type fuse.hcfs"
echo "########## Test delete volume"
$HCFSvol delete autotest
! $HCFSvol list | grep autotest
echo "########## Test terminate hcfs"
result=`$HCFSvol terminate`
grep -q Success <<< "$result"

echo "########## Test mount success"
