#!/bin/bash
# vim:set tabstop=4 shiftwidth=4 softtabstop=0 expandtab:
set -e
# Usage Info

NDK_PATH=/opt/android-ndk-r12b

ErrorReport() {
  local script="$1"
  local parent_lineno="$2"
  local message="$3"
  local code="${4:-1}"
  echo "Error near ${script} line ${parent_lineno}; exiting with status ${code}"
  if [[ -n "$message" ]] ; then
    echo -e "Messageecho ${message}"
  fi
  exit "${code}"
}

# Enable error trace
trap 'ErrorReport "${BASH_SOURCE[0]}" ${LINENO}' ERR
set -e -o errtrace

Usage() {
    local _prog=$(basename $0)
    echo "Usage: $_prog [option]..."
    echo "option:"
    echo "  -push: push gdb version hcfs to device and open gdb console"
    echo "  -sdk <sdk path>: set the SDK path (Default: $SDK_PATH)"
    echo "  -h: show this usage"
    [ "$1" = 0 ] && exit 0 || exit 1
}

CheckParams() {
    while [ "$1" != "" ]; do
        case $1 in
        -h)     Usage 0 ;;
        -ndk)   NDK_PATH=$2; shift ;;
        -push)  PUSH=1 ;;
        *)      Usage 1 ;;
        esac
        shift
    done

    # set PATH
    export PATH=$NDK_PATH/prebuilt/linux-x86_64/bin:$PATH
}

CheckProgram() {
    local _tool=$1
    which "$_tool" > /dev/null 2>&1 || echo "$ERROR_HDR $_tool is not found. Please install it or add its path in \$PATH."
}

CheckTools() {
    local _ret=0
    if ! CheckProgram adb; then
        echo "   Or, install adb by 'apt-get install android-tools-adb'"
        _ret=1
    fi
    if ! CheckProgram fastboot; then
        echo "   Or, install fastboot by 'apt-get install android-tools-fastboot'"
        _ret=1
    fi

    if ! CheckProgram gdb; then
        _ret=1
    elif ! gdb --configuration | grep -q multiarch; then
        echo "gdb doesn't include ndk data, please use -ndk pass android-ndk-r12+ path"
        _ret=1
    fi

    return $_ret
}

# Check parameters
CheckParams "$@"

# Check to see if tools (adb, fastboot) can be found
CheckTools

adb kill-server
adb wait-for-device

SRC=system/bin
for i in linker64;
do
    [ ! -f ./$SRC/$i ] && adb pull /$SRC/$i ./$SRC/
done

SRC=system/lib64
for i in libcurl.so libcrypto.so libsqlite.so libfuse.so libjansson.so \
    libstdc++.so libm.so libc.so libssl.so libz.so libc++.so liblog.so \
    libicuuc.so libicui18n.so libutils.so libbacktrace.so libcutils.so \
    libbase.so libunwind.so libnetd_client.so;
do
    [ ! -f ./$SRC/$i ] && adb pull /$SRC/$i ./$SRC/
done

set -x
if [[ $PUSH ]]; then
    adb wait-for-device
    adb root
    if ! (adb disable-verity | grep -q already); then
        adb reboot
        adb wait-for-device
        adb root
    fi
    adb remount
    adb push system/bin/hcfs /system/bin/
    adb push system/lib64/libfuse.so /system/lib64/
    adb shell 'set `ps | grep /system/bin/hcfs`; su root kill $2'&
    adb reboot
    sleep 40
fi
set +x
X64=64
adb wait-for-device
adb shell 'set `ps | grep /system/bin/hcfs`; su root gdbserver64 --attach :5678 $2'&
sleep 1
adb forward tcp:5678 tcp:5678
cgdb -x gdb.setup
