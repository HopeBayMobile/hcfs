#/bin/bash

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
			NDK_PATH="$OPTARG"
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

	if [[ "$NDK_PATH" = "" ]]; then
		if type -P ndk-build; then
			:
		fi
		echo "ERROR: please specify Android-ndk directory with -d."
		exit 1
	fi
}

if [ -f "$LOCAL_PATH/build/.ndk_path" ]; then
	source $LOCAL_PATH/build/.ndk_path
fi
parse_options "$@"

echo export NDK_PATH="$NDK_PATH" > $LOCAL_PATH/build/.ndk_path
export PATH=$NDK_PATH:$PATH
hash -r

echo "=== Start to build HCFS ==="
src_path=$LOCAL_PATH"/build/HCFS/"
cd $src_path
make

echo "=== Start to build HCFS CLI ==="
src_path=$LOCAL_PATH"/build/HCFS_CLI/"
cd $src_path
make

echo "=== Start to build API Server ==="
src_path=$LOCAL_PATH"/build/API_SERV/"
cd $src_path
make
