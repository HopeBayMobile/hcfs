#!/bin/bash
# vim:set tabstop=4 shiftwidth=4 softtabstop=0 noexpandtab:
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

Usage()
{
cat <<EOF
NAME
	build.sh - HCFS build script

SYNOPSIS
	./build.sh [action] [option]

DESCRIPTION
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
			build userdebug or user type with --userdebug and --user
			respectively. Script will build both type if not specified.
		--test
			test image build process.

	pyhcfs [--test]
		Build python library "pyhcfs" at dist/

	-h
		Show usage
EOF
exit ${1:-0}
}

parse_options()
{
	TARGET=
	TEST=0
	while [[ $# -gt 0 ]]; do
		case $1 in
		lib)      TARGET+="$1;" ;;
		ci-test)  TARGET+="$1;" ;;
		unittest) TARGET+="$1;" ;;
		pyhcfs)   TARGET+="$1;" ;;
		--test)   TEST=1        ;;
		-h)       Usage         ;;
		-d)
			if [ $# -lt 2 ]; then
				echo "Usage: -d <NDK_PATH>"
				Usage 1
			fi
			export SET_NDK_BUILD="$2";
			shift ;;
		*)
			echo "Invalid option -- $@" 2>&1
			Usage 1 ;;
		esac
		shift
	done
}

set_PARALLEL_JOBS()
{
	if hash nproc; then
		_nr_cpu=`nproc`
	else
		_nr_cpu=`cat /proc/cpuinfo | grep processor | wc -l`
	fi
	export PARALLEL_JOBS="-l ${_nr_cpu}.5"
}

unittest()
{
	$repo/tests/unit_test/run_unittests
}

ci-test()
{
	export CI=1
	export CI_VERBOSE=true
	$repo/tests/unit_test/run_unittests
	$repo/tests/ci_code_report.sh
}

lib()
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

pyhcfs ()
{
	$repo/utils/setup_dev_env.sh -m docker_host
	docker pull docker:5000/docker_hcfs_test_slave
	set -x

	if [ "$TEST" -eq 1 ]; then
		PYHCFS_TARGET=test
	else
		PYHCFS_TARGET=bdist_egg
	fi

	if [ -e /.docker* ]; then
		$repo/utils/setup_dev_env.sh -m docker_host
		python3 setup.py $PYHCFS_TARGET
	else
		docker run --rm -v "$repo":/hcfs docker:5000/docker_hcfs_test_slave \
			bash -c "cd /hcfs; umask 000; python3 setup.py $PYHCFS_TARGET"
	fi

	if [ -d dist/ ]; then
		mkdir -p build/out/
		rsync -arcv --no-owner --no-group --no-times dist/ build/out/
	fi
}

parse_options "$@"

# setup -jN for make
set_PARALLEL_JOBS

# Running target
eval ${TARGET:=Usage}
