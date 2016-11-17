#!/bin/bash
# vim:set tabstop=4 shiftwidth=4 softtabstop=0 noexpandtab:

# Load Default Value
: "${TARGET_ARCH:=64}"

ErrorReport()
{
	local script="$1"
	local parent_lineno="$2"
	local code="${3:-1}"
	echo
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
	while (( $# > 0 )); do
		case $1 in
		-h)        Usage 0;;
		-ndk)
			if (( $# < 2 )); then
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

	# Customize PATH, later path has higher priority
	local ENV_PATH=()
	ENV_PATH+=("/opt/android-*")
	ENV_PATH+=("${NDK_PATH:-}")
	ENV_PATH+=("${NDK_PATH_SET:-}")

	local EXPANDED=()
	for E in "${ENV_PATH[@]}"; do
		EXPANDED+=("${E}")
		EXPANDED+=("${E}/prebuilt/linux-x86_64/bin")
		EXPANDED+=("${E}/platform-tools")
	done
	for E in ${EXPANDED[@]}
	do
		if [[ -d "$E" && ! ":$PATH:" == *":$E:"* ]]; then
			export PATH="$E:$PATH"
		fi
	done
}

CheckProgram() {
	local _tool=$1
	if ! which "$_tool" > /dev/null 2>&1; then
		echo "Error, $_tool is not found. $2"
		exit 1
	fi
}

CheckTools() {
	local _ret=0
	CheckProgram adb "Please install adb from android-sdk"
	CheckProgram ndk-build "Please install android-ndk, set -ndk <NDK_PATH> OR export NDK_PATH"

	if ! [[ $(gdb --configuration) = *android* ]]; then
		echo "Error: gdb in PATH ( $(\which gdb) ) is not android-ndk version, which" \
			"is located at android-ndk-r*/prebuilt/linux-x86_64/bin/" && false
	fi
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
	echo ">>   Check required library files. Pull files if they are missing or mismatch"
	local REQUIREMENT=()
	REQUIREMENT+=("system/bin/linker${TARGET_ARCH}")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libstdc++.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libm.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libc.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libssl.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libz.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libc++.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/liblog.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libicuuc.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libicui18n.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libutils.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libbacktrace.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libcutils.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libbase.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libunwind.so")
	REQUIREMENT+=("system/lib${TARGET_ARCH}/libnetd_client.so")

	CHANGELIST=$(ANDROID_PRODUCT_OUT=. adb sync -l system 2>&1)
	for F in "${REQUIREMENT[@]}"; do
		if [[ ! -f ./$F || $CHANGELIST = *$F* ]]; then
			adb pull -a "/$F" "./$F" || :
		fi
	done
}

PushGDBbinary() {
	echo ">> [PushGDBbinary]"
	adb wait-for-device
	adb root
	adb wait-for-device
	if ! adb disable-verity | grep -q already; then
		echo ">> Disable-verity and reboot"
		adb reboot
		adb wait-for-device
		adb root
	fi
	adb remount
	echo ">>   Push hcfs"
	OUT=$(ANDROID_PRODUCT_OUT=. adb sync system | tee /dev/fd/2)
	if [[ ! $OUT = *" 0 files pushed"* ]]; then
		echo ">> File changed. Reboot."
		adb shell 'set `ps | grep /system/bin/hcfs`; su root kill $2'&
		adb reboot
	fi
}

StartGDB() {
	echo ">> [StartGDB]"
	adb wait-for-device
	printmesg="echo >> Wait /storage/emulated being mounted"
	until [[ -n `adb shell "mount |grep /storage/emulated"` ]]; do
		$printmesg
		printmesg=true
		sleep 1; echo -n .
	done
	sleep 5; echo Done
	unset printmesg
	adb forward tcp:5678 tcp:5678
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

#PullGDBFiles
#if [[ $PUSH = 1 || $PUSHONLY = 1 ]]; then
#	PushGDBbinary
#fi
if [[ $PUSHONLY != 1 ]]; then
	StartGDB
fi

