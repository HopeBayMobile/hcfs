#!/bin/bash
echo ======== ${BASH_SOURCE[0]} ========
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../.. && pwd )"
set -x -e

# Setup Test Env
. $WORKSPACE/utils/setup_dev_env.sh -m functional_test

# Compile binary files
make -C $WORKSPACE/src/HCFS
make -C $WORKSPACE/src/CLI_utils

# Setup PATH for test
if ! grep -E "(^|:)$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils(:|$)" <<< `echo $PATH`; then
	export PATH="$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils:$PATH"
fi

# Simple mount & umount test with docker aerofs/swift
if [ "$mode" = "docker" ]; then
	# Wait swift ready
	SWIFT="swift -A http://swift_test:8080/auth/v1.0 -U test:tester -K testing"
	while ! $SWIFT stat; do sleep 1; done
	# Create Container
	while $SWIFT post autotest_private_container |& grep failed; do sleep 1; done
	mkdir -p $WORKSPACE/tmp/{meta,block,mount}
	sudo rm -rf $WORKSPACE/tmp/{meta,block,mount}/*
	sudo tee /etc/hcfs.conf <<-EOF
	METAPATH= $WORKSPACE/tmp/meta
	BLOCKPATH = $WORKSPACE/tmp/block
	CACHE_SOFT_LIMIT = 53687091
	CACHE_HARD_LIMIT = 107374182
	CACHE_DELTA = 10485760
	MAX_BLOCK_SIZE = 1048576
	CURRENT_BACKEND = swift
	SWIFT_ACCOUNT = test
	SWIFT_USER = tester
	SWIFT_PASS = testing
	SWIFT_URL = swift_test:8080
	SWIFT_CONTAINER = autotest_private_container
	SWIFT_PROTOCOL = http
	LOG_LEVEL = 10
	EOF
	hcfs &
	while ! HCFSvol create autotest | grep Success; do sleep 1 ;done
	HCFSvol mount autotest $WORKSPACE/tmp/mount
	mount | grep "hcfs on $WORKSPACE/tmp/mount type fuse.hcfs"
	HCFSvol terminate
	! mount | grep "hcfs on $WORKSPACE/tmp/mount type fuse.hcfs"
fi

#python pi_tester.py -s TestSuites/HCFS.csv
