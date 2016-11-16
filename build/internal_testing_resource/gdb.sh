#!/bin/bash
# vim:set tabstop=4 shiftwidth=4 softtabstop=0 noexpandtab:

# Load Default Value
: "${TARGET_ARCH:=64}"

ErrorReport()
{
	local script="$1"
	local parent_lineno="$2"
	local code="${3:-1}"
	eval printf %.0s- '{1..'"${COLUMNS:-$(tput cols)}"\}; echo
	echo "Error is near ${script} line ${parent_lineno}. Return ${code}"
	local Start
	local Point
	if [[ $parent_lineno -lt 1 ]]; then
		Start=$parent_lineno
		Point=1
	else
		Start=$((parent_lineno-2))
		Point=3
	fi
	local End=$((parent_lineno+2))
	cat -n "${script}" \
		| sed -n "${Start},${End}p" \
		| sed $Point"s/^  / >/"
	eval printf %.0s- '{1..'"${COLUMNS:-$(tput cols)}"\}; echo
	exit "${code}"
}

# Enable error trace
trap 'ErrorReport "${BASH_SOURCE[0]}" ${LINENO} $?' ERR
set -o pipefail  # trace ERR through pipes
set -o errtrace  # trace ERR through 'time command' and other functions
set -o nounset   ## set -u : exit the script if you try to use an uninitialised variable
set -o errexit   ## set -e : exit the script if any statement returns a non-true return value

Usage() {
    local _prog
    _prog="$(basename "$0")"
    cat <<EOU
NAME
    gdb.sh - Start GDB session to debug hcfs on Android device

SYNOPSIS
    $_prog [option]...

DESCRIPTION
    -h
        Show this usage

    -push
        Push hcfs and .so files into device and reboot

    -pushonly
        Push hcfs and .so files into device only

	-ndk <NDK_PATH>
			set the NDK path
EOU
	exit ${1:-0}
}

CheckParams() {
	PUSHONLY=0
	PUSH=0
	while [ "${1:-}" != "" ]; do
		case $1 in
		-h)        Usage 0;;
		-ndk)
			if [ $# -lt 2 ]; then
				echo "Usage: -ndk <NDK_PATH>"
				exit 1
			fi
			NDK_PATH_SET="$2"
			shift ;;
		-push)     PUSH=1;;
		-pushonly) PUSHONLY=1;;
		*)         echo ls: invalid option -- \'"$1"\'; Usage 1;;
		esac
		shift
	done

	# set PATH
	if [ -z "${NDK_PATH_SET:=}" -a -z "$NDK_PATH" ] ; then
		echo "Error: require NDK_PATH, use -ndk <NDK_PATH> OR export NDK_PATH before execute script"
		exit 1
	fi
	if [ -n "$NDK_PATH" ]; then
		export PATH=$NDK_PATH/prebuilt/linux-x86_64/bin:$PATH
	fi
	if [ -n "$NDK_PATH_SET" ]; then
		export PATH=$NDK_PATH_SET/prebuilt/linux-x86_64/bin:$PATH
	fi
}

CheckProgram() {
	local _tool=$1
	which "$_tool" > /dev/null 2>&1 || echo "$ERROR_HDR $_tool is not found. Please install it or add its path in \$PATH."
}

CheckTools() {
	local _ret=0
	if ! CheckProgram adb; then
		echo "   Or, install adb from android-sdk"
		_ret=1
	fi
	if ! CheckProgram fastboot; then
		echo "   Or, install adb from android-sdk"
		_ret=1
	fi

	if ! CheckProgram gdb; then
		_ret=1
	elif ! gdb --configuration | grep -q multiarch; then
		which gdb
		echo ""
		echo "Error: gdb doesn't include ndk data, please use -ndk pass android-ndk-r12+ path"
		echo Gdb path found in script: "$(which gdb)"
		_ret=1
	fi
		which gdb
	if which cgdb > /dev/null 2>&1; then
		GDB=cgdb
	else
		GDB=gdb
	fi

	return $_ret
}

PullGDBFiles() {
	echo ">> [PullGDBFiles]"
	adb wait-for-device
	SRC=system/bin
	for i in linker${TARGET_ARCH};
	do
		if [ ! -f "./$SRC/$i" ]; then
			adb pull "/$SRC/$i" "./$SRC/"
		fi
	done

	echo ">>   Check required library files. Pull files if they are missing or mismatch"
	CHANGELIST=$(ANDROID_PRODUCT_OUT=. adb sync -l system 2>&1)
	SRC=system/lib${TARGET_ARCH}
	for i in libcrypto.so libsqlite.so libstdc++.so libm.so libc.so libssl.so \
	libz.so libc++.so liblog.so libicuuc.so libicui18n.so libutils.so \
	libbacktrace.so libcutils.so libbase.so libunwind.so libnetd_client.so;
	do
		if [[ ! -f ./$SRC/$i || $CHANGELIST = *$i* ]]; then
			adb pull -a "/$SRC/$i" "./$SRC/" || :
		fi
	done
}

PushGDBbinary() {
	echo ">> [PushGDBbinary]"
	adb wait-for-device
	adb root
	adb wait-for-device
	if ! adb disable-verity | grep -q already; then
		echo ">>   Disable-verity and reboot"
		adb reboot
		adb wait-for-device
		adb root
	fi
	adb remount
	echo ">>   Push hcfs"
	OUT=$(ANDROID_PRODUCT_OUT=. adb sync system)
	if [[ ! $OUT = *"0 files pushed"* ]]; then
		echo ">>   Some files pushed, Reboot"
		adb shell 'set `ps | grep /system/bin/hcfs`; su root kill $2'&
		adb reboot
	fi
}

StartGDB() {
	echo ">> [StartGDB]"
	adb wait-for-device
	adb forward tcp:5678 tcp:5678
	adb shell 'set `ps | grep gdbserver`; [ -n "$2" ] && su root kill $2'
	adb shell 'set `ps | grep /system/bin/hcfs`; su root gdbserver --attach :5678 $2'&
	gdbserverpid=$!
	sleep 1
	$GDB -x gdb.setup
	kill $gdbserverpid || :
}

# Check parameters
CheckParams "$@"

# Check to see if tools (adb, fastboot) can be found
CheckTools

# Main scripts

PullGDBFiles
if [[ $PUSH = 1 || $PUSHONLY = 1 ]]; then
	PushGDBbinary
fi
if [[ $PUSHONLY != 1 ]]; then
	StartGDB
fi

