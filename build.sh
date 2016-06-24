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

echo -e "======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $repo

function usage()
{
sed -e "s/\t/    /g" <<EOF
Usage: ./build.sh [ACTION] [OPTIONS]

ACTION:
	lib [-d ndk-path]
		Build Android Libraries. ndk-path is path to Android-ndk directory
	ci-test
		Run continuous integration tests.
	unittest
		Run unit tests.
	image 5x|s58a [--userdebug|--user] [--test]
		5x|s58a
			Build Android image.
		--userdebug|--user
			buila userdebug or user type with --userdebug and --user
			respectively. Script will build both type if not specified.
		--test
			test image build process.
	pyhcfs [--test]
		Build python library "pyhcfs" at dist/
OPTIONS:
	-h
		Show usage
EOF
}

function parse_options()
{
	TEST=0
	while [[ $# -gt 0 ]]; do
		case $1 in
		lib)
			TARGET="$1"; shift ;;
		ci-test)
			TARGET="$1"; shift ;;
		unittest)
			TARGET="$1"; shift ;;
		pyhcfs)
			TARGET="$1"; shift ;;
		-d )
			if [ -z "$2" ] ; then echo "Invalid argument for -d"; usage; exit 1; fi
			export SET_NDK_BUILD="$2"; shift 2 ;;
		-h )
			usage; exit ;;
		--test)
			TEST=1; shift 1;;
		*)
			exec 2>&1 ;echo "Invalid option: $@"; usage; exit 1 ;;
		esac
	done
}

function set_PARALLEL_JOBS()
{
	_nr_cpu=`cat /proc/cpuinfo | grep processor | wc -l`
	PARALLEL_JOBS=-j`expr $_nr_cpu + $_nr_cpu`
}

function unittest()
{
	$repo/tests/unit_test/run_unittests
}
function ci-test()
{
	export CI_VERBOSE=true
	export UNITTEST_MAKE_FLAG=-k
	$repo/containers/hcfs-test-slave/ci-test.sh
}

function lib()
{
	# load NDK_BUILD
	# compress with password protection
	packages+=" zip"

	# speed up compiling
	packages+=" ccache"

	install_pkg
	cd build
	set -x
	make $PARALLEL_JOBS
	exit
}

function pyhcfs ()
{
	$repo/utils/setup_dev_env.sh -m docker_host
	if ! groups $USER | grep  -q "\(docker\|root\)"; then
		echo To run docker with user, please add your user into docker group and re-login session:
		echo "sudo usermod -aG docker <user_name>"
		exit 1
	fi
	docker pull docker:5000/docker_hcfs_test_slave
	set -x
	if [ "$TEST" -eq 1 ]; then
		docker run --rm -v "$repo":/hcfs docker:5000/docker_hcfs_test_slave \
			bash -c "cd /hcfs; umask 000; python setup.py test"
	else
		docker run --rm -v "$repo":/hcfs docker:5000/docker_hcfs_test_slave \
			bash -c "cd /hcfs; umask 000; python setup.py bdist_egg"
		mkdir -p build/out/
		rsync -arcv --no-owner --no-group --no-times dist/ build/out/
	fi
}

parse_options "$@"

# setup -jN for make
set_PARALLEL_JOBS

# Running target
if [ -n "$TARGET" ]; then
	eval $TARGET
else
	usage
fi
