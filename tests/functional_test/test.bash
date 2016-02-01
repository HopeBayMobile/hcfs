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

echo "########## Setup Test Env"
$repo/utils/setup_dev_env.sh -m functional_test
. $repo/utils/env_config.sh

if [[ $(id -Gn) != *fuse* ]]; then
	echo "########## Reload script with fuse group permission"
	exec sg fuse "${BASH_SOURCE[0]}"
fi
id
groups

# Main cource code
function cleanup {
	echo "########## Cleanup"
	sudo umount $repo/tmp/mount* |& grep -v "not mounted" || :
	sudo pgrep -a "hcfs " || :
	sudo pkill 'hcfs' || :
	mkdir -p $repo/tmp/{meta,block,mount}
	sudo find $repo/tmp/{meta,block,mount*} -mindepth 1 -delete
}
trap cleanup EXIT

# Setup for $hcfs and $HCFSvol
$here/TestCases/HCFS/compile_hcfs_bin.bash
. $here/TestCases/HCFS/path_config.sh

echo "########## Current hcfs daemons"
pgrep -a -f ".*hcfs (.*|$)" || :

s=4G
prefix="$here/TestCases/HCFS/TestSamples"
mkdir -p $prefix
if [ ! -f $prefix/$s ]; then
	echo "########## Prepare large file: $s"
	bs=4K
	count=`units ${s}iB ${bs}iB -1 -t --out="%.f"`
	openssl enc -aes-256-ctr -pass pass:`date +%s%N` -nosalt < /dev/zero 2>/dev/null | dd iflag=fullblock bs=$bs count=$count | tee $prefix/$s | pv -fs $s | md5sum | sed -e "s/-/$s/" > $prefix/${s}.md5
	#openssl enc -aes-256-ctr -pass pass:`date +%s%N` -nosalt < /dev/zero 2>/dev/null | dd iflag=fullblock bs=$bs count=$count | pv -s $s > $prefix/$s
fi

echo "########## pi_tester.py"
cd "$here"
python pi_tester.py -d debug -s TestSuites/HCFS.csv
