#!/bin/bash
echo -e "\n======== ${BASH_SOURCE[0]} ========"
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../../.. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

if [ $(id -gn) != fuse ]; then
	echo "########## Setup Test Env"
	$WORKSPACE/utils/setup_dev_env.sh -m functional_test
	echo "########## Reload script with fuse group permission"
	exec sg fuse "${BASH_SOURCE[0]}"
fi
. $WORKSPACE/utils/env_config.sh
. $WORKSPACE/utils/trace_error.bash
set -e

function cleanup {
	echo "########## Cleanup"
	sudo umount $WORKSPACE/tmp/mount* |& grep -v "not mounted" || :
	sudo pgrep -a "hcfs " || :
	sudo pkill 'hcfs' || :
	sudo rm -rf $WORKSPACE/tmp/{meta,block,mount*}
	mkdir -p $WORKSPACE/tmp/{meta,block,mount}
}
trap cleanup EXIT

echo "########## Compile binary files"
make -s -C $WORKSPACE/src/HCFS clean
make -s -C $WORKSPACE/src/CLI_utils clean
CFLAGS_ARG="`sed -rn -e '/^CFLAGS/s/ -Wall| -Wextra//g' \
	-e "/^CFLAGS/s/ *= *(.*)/='\1 -w'/p" $WORKSPACE/src/HCFS/Makefile`"
eval "make -s -C $WORKSPACE/src/HCFS $CFLAGS_ARG"

CFLAGS_ARG="`sed -rn -e '/^CFLAGS/s/ -Wall| -Wextra//g' \
	-e "/^CFLAGS/s/ *= *(.*)/='\1 -w'/p" $WORKSPACE/src/CLI_utils/Makefile`"
eval "make -s -C $WORKSPACE/src/CLI_utils $CFLAGS_ARG"

echo "########## Setup PATH for test"
if ! grep -E "(^|:)$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils(:|$)" <<< `echo $PATH`; then
	export PATH="$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils:$PATH"
	hash -r
fi

hcfs=`type -a hcfs | sed -s 's/.* is //'`
HCFSvol=`type -a HCFSvol | sed -s 's/.* is //'`

cleanup

echo "########## Start hcfs deamon"
$hcfs $WORKSPACE/tmp -oallow_other &
sleep 1

echo "########## Create hcfs volume"
while :; do
	result=`$HCFSvol create autotest`
	echo $result
	if grep -q Success <<< "$result"; then
		break
	fi
	sleep 1
done

echo "########## Test Mount"
$HCFSvol mount autotest $WORKSPACE/tmp/mount
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
! HCFSvol list | grep autotest
echo "########## Test terminate hcfs"
result=`$HCFSvol terminate`
grep -q Success <<< "$result"

echo "########## Test mount success"
