#!/bin/bash

LOCAL_PATH="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $LOCAL_PATH

function usage()
{
	echo "Usage: ./build.sh [OPTIONS]"
	echo "Build Android Libraries."
	echo
	echo "Required options:"
	echo "  -d DIR     Path to Android-ndk directory"
	echo
	echo "Optional options:"
	echo "  -h         Show usage"
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

if [ -f "$LOCAL_PATH/build/.ndk_build" ]; then
	source $LOCAL_PATH/build/.ndk_build
fi
parse_options "$@"

if [ -f "$NDK_BUILD" ]; then
	echo export NDK_BUILD="$NDK_BUILD" > $LOCAL_PATH/build/.ndk_build
fi

cd $LOCAL_PATH"/build/"
make
