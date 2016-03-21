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
	echo "Usage: ./build.sh [OPTIONS] TARGET"
	echo "Available Targets:"
	echo "  android-lib   Build Android Libraries."
	echo "  ci-test       Run continuous integration tests."
	echo "  unittest      Run unit tests."
	echo
	echo "Options:"
	echo "  -d DIR   Path to Android-ndk directory"
	echo "  -h       Show usage"
}

function parse_options()
{
	while [[ $# -gt 0 ]]; do
		opt="$1";
		shift; #expose next argument
		case $opt in
		lib|ci-test|unittest)
			TARGET="$opt";;
		"-d")
			export SET_NDK_BUILD="$1";
			shift;;
		"-h")
			usage
			exit;;
		*)
			echo >&2 "Invalid option: $@";
			exit 1;;
		esac
	done

}

parse_options "$@" && :

case "$TARGET" in
unittest)
	$repo/tests/unit_test/run_unittests
	;;
ci-test)
	export CI_VERBOSE=true
	export UNITTEST_MAKE_FLAG=-k
	$repo/containers/hcfs-test-slave/ci-test.sh
	;;
lib)
	# load NDK_BUILD
	packages+=" zip"		# compress with password protection
	install_pkg
	if [ -f build/.ndk_build ]; then
		source $repo/build/.ndk_build;
	fi
	FIND_NDK_BUILD=`/usr/bin/which ndk-build` || :
	NDK_BUILD_EXIST=false
	for i in "$SET_NDK_BUILD" "$NDK_BUILD" "$FIND_NDK_BUILD"; do
		if [ -f "$i" ]; then
			export NDK_BUILD="$i"
			echo export NDK_BUILD="$i" > $repo/build/.ndk_build
			NDK_BUILD_EXIST=true
			break
		fi
	done
	if ! $NDK_BUILD_EXIST; then
		echo "ERROR: please specify filepath of ndk-build with -d PATH/TO/android-ndk-r10e/ndk-build."
		exit 1
	fi
	cd $repo"/build/"
	_nr_cpu=`cat /proc/cpuinfo | grep processor | wc -l`
	PARALLEL_JOBS=-j`expr $_nr_cpu + $_nr_cpu`
	set -x
	make $PARALLEL_JOBS
	exit
	;;
*)
	usage
	exit;;
esac
