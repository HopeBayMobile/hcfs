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

function usage()
{
	echo "Usage: ./build.sh [OPTIONS] TARGET"
	echo "Available Targets:"
	echo "  android-lib   Build Android Libraries."
	echo "  ci-test       Run continuous integration tests."
	echo
	echo "Options:"
	echo "  -d DIR   Path to Android-ndk directory"
	echo "  -h       Show usage"
}

function parse_options()
{
	local OPTIND=1
	local ORIG_ARGV
	local opt
	while getopts "d:h" opt; do
		case "$opt" in
		d)
			export NDK_BUILD="$OPTARG"
			;;
		h)
			usage
			exit
			;;
		*)
			return 1
			;;
		esac
	done

	(( OPTIND -= 1 )) || true
	shift $OPTIND || true
	ORIG_ARGV=("$@")

	if [[ "$NDK_BUILD" = "" ]]; then
		if /usr/bin/which ndk-build; then
			export NDK_BUILD=`/usr/bin/which ndk-build`
		else
			echo "ERROR: please specify filepath of ndk-build with -d."
			exit 1
		fi
	fi
}

if [ -f "$repo/build/.ndk_build" ]; then
	source $repo/build/.ndk_build
fi
parse_options "$@"

if [ -f "$NDK_BUILD" ]; then
	echo export NDK_BUILD="$NDK_BUILD" > $repo/build/.ndk_build
fi

case "$1" in
ci-test)
	. tests/docker_test.sh
	;;
android-lib)
	cd $repo"/build/"
	make
	exit
	;;
*)
	usage
	exit 1
	;;
esac
