#!/bin/bash
# vim:set tabstop=2 shiftwidth=2 softtabstop=0 expandtab:
#
# Description: # This is to apply opengapps into Nexus devices with AOSP ROM
#
# Authors: William W.-Y. Liang, Can Yu, etc
#          from Hope Bay Tech, Copyright (C) 2016-
# Contact: william.liang@hopebaytech.com
#          can.yu@hopebaytech.com
#
# Date: $Date: 2016/07/13 17:17:00 $
# Version: $Revision: 1.13 $
#
# History:
#
# $Log: nexus-install-gapps.sh,v $
# Revision 1.13  2016/07/13 17:17:00  jethro
# Support Ubuntu VM in virtualbox, use local cache fro gapps
#
# Revision 1.12  2016/07/13 07:24:15  wyliang
# Support Apple MacOS
#
# Revision 1.11  2016/07/11 12:11:00  wyliang
# Add the '-boot4perm' option to support permission grant by a userdebug boot.img; Partial code refactoring
#
# Revision 1.10  2016/06/23 11:27:29  wyliang
# Support '-gp' to grant permissions (only); Refine message and sideload check time
#
# Revision 1.9  2016/06/21 09:06:57  wyliang
# Let -rom and -rom-zip imply -fr, and -factory and -factory-url imply -ff; Detect 'adb-disabled' on reflashing images; Minor refactoring on messages
#
# Revision 1.8  2016/06/17 07:24:14  wyliang
# Support '-rom-zip' to flash a full zipped image; Improve the adb mode checking mechanism; Partial message refactoring
#
# Revision 1.7  2016/06/14 11:49:15  wyliang
# Reduce the chance to be blocked at sideload stage
#
# Revision 1.6  2016/06/14 09:55:40  wyliang
# Refine messages
#
# Revision 1.5  2016/06/09 07:51:10  wyliang
# Add version option '-v'
#
# Revision 1.4  2016/06/09 07:40:07  wyliang
# Add tool checking; Refine message; Refind code; Support twrp sideload command and checkings; Grant gapps with more permissions
#
# Revision 1.3  2016/06/08 11:56:28  wyliang
# Fix minor bugs; Add some checks; Re-org a portion of the source code
#
# Revision 1.2  2016/06/08 09:32:29  wyliang
# Support the feature to download and cache files from the original servers
#
# Revision 1.1  2016/06/07 06:04:50  wyliang
# Create the nexus gapps installation script
#

# Android SDK path

SDK_PATH=$HOME/android-sdk-linux

# Recovery and OpenGapps links (and md5 file)

RECOVERY_URL=https://dl.twrp.me/bullhead/twrp-3.0.2-0-bullhead.img
RECOVERY_FILE=$(basename $RECOVERY_URL)
GAPPS_URL=https://github.com/opengapps/arm64/releases/download/20160710/open_gapps-arm64-6.0-pico-20160710.zip
GAPPS_URL=ftp://nas/ubuntu/CloudDataSolution/HCFS_android/resources/open_gapps-arm64-6.0-pico-20160710.zip
GAPPS_FILE=$(basename $GAPPS_URL)
FACTORY_URL=https://dl.google.com/dl/android/aosp/bullhead-mtc19v-factory-f3a6bee5.tgz
FACTORY_FILE=$(basename $FACTORY_URL)
FACTORY_DIR=$(echo $FACTORY_FILE | awk -F- '{ print $1"-"$2 }')
ROM_DIR=tera
ROM_ZIP=
BOOT4PERM_IMG=
HERE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Usage Info

Usage() {
  local _prog=$(basename $0)
  echo "Usage: $_prog [option]..."
  echo "option:"
  echo "  -fr: flash ROM, from the source specified by either '-rom-zip' or '-rom', where '-rom-zip' is used first."
  echo "  -ff: flash factory image ONLY"
  echo "  -rom <rom dir>: specify the directory containing ROM images (Default <rom dir>: $ROM_DIR). The '-fr' option is implied."
  echo "  -rom-zip <rom zip>: specify the ROM's zip image file (Default <rom zip>: N/A). The '-fr' option is implied."
  echo "  -factory <factory image>: specify the factory image tar ball (e.g. <factory image>: $FACTORY_FILE). The '-ff' option is implied."
  echo "  -factory-url <factory URL>: specify the factory image URL (Default <factory URL>: $FACTORY_URL). The '-ff' option is implied."
  echo "  -gapps <gapps zip>: specify the gapps zip file name (e.g. <gapps zip>: $GAPPS_FILE)"
  echo "  -gapps-url <gapps URL>: specify the gapps URL (Default <gapps URL>: $GAPPS_URL)"
  echo "  -recovery <recovery image>: specify the recovery image file name (e.g. <recovery image>: $RECOVERY_FILE)"
  echo "  -recovery-url <recovery URL>: specify the recovery URL (Default <recovery URL>: $RECOVERY_URL)"
  echo "  -boot4perm <boot image>: specify the temporary boot image for granting permissions"
  echo "  -sdk <sdk path>: set the SDK path (Default: $SDK_PATH)"
  echo "  -ask: ask the user to confirm every major step (Default: do no ask)"
  echo "  -fd: force to download again (for recovery and gapps)"
  echo "  -gp: grant permission ONLY"
  echo "  -v: show the version"
  echo "  -h: show this usage"
  echo "Examples:"
  echo "  Install Open Gapps only: "
  echo "    $ $_prog"
  echo "  Flash ROM images in a directory named 'tera-1060-userdebug/' and then install Open Gapps:"
  echo "    $ $_prog -rom tera-1060-userdebug"
  echo "  Flash ROM images under 'tera-1060-user/', with a temporary boot image 'userdebug/boot.img' to grant permissions, and then install Open Gapps:"
  echo "    $ $_prog -rom tera-1060-user -boot4perm userdebug/boot.img"
  echo "  Flash ROM images under the default directory '$ROM_DIR/' and then install Open Gapps:"
  echo "    $ $_prog -fr"
  echo "  Flash a full ROM images specified by a zip file and then install Open Gapps:"
  echo "    $ $_prog -rom-zip aosp_bullhead-img-874.zip"
  echo "  Restore the Nexus factory images:"
  echo "    $ $_prog -ff"
  echo "  Restore the Nexus factory images using a pre-downloaded file 'bullhead-mxy12z.tgz':"
  echo "    $ $_prog -factory bullhead-mxy12z.tgz"
  echo "  Install Open Gapps using the file my-gapps.zip:"
  echo "    $ $_prog -gapps my-gapps.zip"
  [ "$1" = 0 ] && exit 0 || exit 1
}

# Default actions

FLASH_FACTORY=0
FLASH_ROM=0
NEED_CONFIRM=0
FORCE_DOWNLOAD=0
FORCE_GRANT_PERM=0
DO_BOOT4PERM=0

ERROR_HDR="** Error:"

# Tools for different host environment
HOST_OS="$(uname -s)"

# ---------
# Functions
# ---------

# Primary Functions

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

ReloadUSB() {
  case $HOST_OS in
  Linux)
    # setup udev rules
    if [ -f "$HERE/utils/51-android.rules" ]; then
      sudo cp -u "$HERE/utils/51-android.rules" /etc/udev/rules.d/51-android.rules
    fi
    sudo sh -c "(udevadm control --reload-rules && udevadm trigger --action=change)"
    ;;
  esac
}

Error() {
  [ "$1" = "newline" ] && { echo; shift; }
  echo "$ERROR_HDR $*"
  echo "Hint: type '$(basename $0) -h' to check the usage of the command."
  exit 1
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

  # Apple consideration
  if [ "$HOST_OS" = "Darwin" ]; then
    TOOL_MD5="md5 -q"
    SED_OPT="-E"
  else
    TOOL_MD5=md5sum
    SED_OPT="-r"
  fi

  return $_ret
}

CheckDir () {
  local _file=$1
  local _show=$2

  if [ -d "$_file" ]; then
    return 0
  else
    # show message only when $_show == 0
    [ "$_show" != "probe-only" ] && Error "the directory '$_file' doesn't exist!"
    return 1
  fi
}

CheckFile () {
  local _file=$1
  local _show=$2

  if [ -f "$_file" ]; then
    return 0
  else
    # show message only when $_show == 0
    [ "$_show" != "probe-only" ] && Error "the file '$_file' doesn't exist!"
    return 1
  fi
}

ToConfirm() {
  local _force=$1

  if [ "$_force" = "force" -o "$NEED_CONFIRM" -eq 1 ]; then
    { printf "Press enter when ready..." && read; }
  fi
}

Sleep() {
  local _secs=$1

  for ((i=0; i<$_secs; i++)); do
    printf "." && sleep 1
  done
}

Remove2Bak() {
  local _file=$1
  [ -f "$_file" ] && mv -f "$_file" "$_file".bak
}

CheckMD5() {
  local _file=$1
  local _md5a="$($TOOL_MD5 $_file | awk '{ print $1 }')"
  local _md5b="$(awk '{ print $1 }' $_file.md5)"

  printf ">> Check the MD5 checksum of '$_file'..."

  # if ! md5sum -c "$_file.md5"; then # this is not used because some .md5 file doesn't match the format
  if [ "$_md5a" != "$_md5b" ]; then
    echo "$ERROR_HDR MD5 check failed for '$_file'! Remove '$_file' and the md5 to .bak file.\n"
    Remove2Bak "$_file"
    Remove2Bak "$_file.md5"
    exit 1
  fi
  echo "done"
}

Download() {
  local _url=$1
  local _file=$2
  local _url_file="$(basename $_url)"

  if echo ${_url,,} | grep twrp > /dev/null 2>&1; then
    # deal with twrp's http referer issue
    curl $_url -H "Referer: $_url.html" > $(basename $_url)
  else
    wget "$_url"
  fi

  if [ "$?" -eq 0 ]; then
    [ "$_url_file" != "$_file" ] && mv -f "$_url_file" "$_file"
    return 0
  fi
}

GetFile () {
  local _url=$1
  local _file=$2

  # if force to download, rename existing file to .bak file
  [ "$FORCE_DOWNLOAD" -eq 1 ] && { Remove2Bak "$_file"; Remove2Bak "$_file.md5"; }

  # download if not exist
  if [ ! -f "$_file" ]; then
    echo ">> Download $_url."
    if Download "$_url" "$_file"; then
      echo "done"
    else
      Error newline "failed to download the file '$_url'!"
    fi
  fi

  # optionally download and check md5
  if [ -f "$_file.md5" ] || wget "$_url.md5"; then
    CheckMD5 "$_file" "$_file.md5"
  fi

  return 0
}

GetMode() {
  local mode=$({ adb devices; fastboot devices; } | tr -d "\n"| sed $SED_OPT -n "s/.*(fastboot|device|recovery|sideload).*/\1/p")
  printf ${mode:-unknown}
}

CheckMode() {
  local _mode=$1

  { adb devices; fastboot devices; } 2>&1 | egrep "$_mode" > /dev/null 2>&1
}

WaitMode () {
  local _mode="$1"
  local _timeout="$2"
  local _i

  for ((_i=0; _i<$_timeout; _i++)); do
    if CheckMode "$_mode"; then
      break
    else
      ReloadUSB
      Sleep 1
    fi
  done
  Sleep 2

  [ "$_i" -lt "$_timeout" ] && { echo "done"; return 0; } || { echo "stop waiting"; return 1; }
}

Wait4App2Appear() {
  local _app=$1
  local _timeout="$2"
  local _i

  printf ">> Wait for the App '$_app' to get ready"

  for ((_i=0; _i<$_timeout; _i++)); do
    adb shell pm list packages 2> /dev/null | grep $_app > /dev/null > /dev/null 2>&1 && break || Sleep 1
  done
  Sleep 2

  [ "$_i" -lt "$_timeout" ] && { echo "done"; return 0; } || { echo "stop waiting"; return 1; }
}

# Major Functions

CheckParams() {
  while [ "$1" != "" ]; do
    case $1 in
    -h)
      Usage 0
      ;;
    -v)
      grep "\$Revision" $0 | head -1 | awk '{ print "version", $4 }'
      exit 0
      ;;
    -ff)
      FLASH_FACTORY=1
      ;;
    -fr)
      FLASH_ROM=1
      ;;
    -ask)
      NEED_CONFIRM=1
      ;;
    -sdk)
      SDK_PATH=$2
      shift
      ;;
    -fd)
      FORCE_DOWNLOAD=1
      ;;
    -gp)
      # FORCE_GRANT_PERM=1
      GrantPermissions
      exit 0
      ;;
    -rom)
      CheckDir "$2"
      ROM_DIR=$2
      FLASH_ROM=1	# implied option: -fr
      shift
      ;;
    -rom-zip)
      CheckFile "$2"
      ROM_ZIP=$2
      FLASH_ROM=1	# implied option: -fr
      shift
      ;;
    -recovery)
      CheckFile "$2"
      RECOVERY_FILE=$2
      shift
      ;;
    -recovery-url)
      RECOVERY_URL=$2
      shift
      ;;
    -gapps)
      CheckFile "$2"
      GAPPS_FILE=$2
      shift
      ;;
    -gapps-url)
      GAPPS_URL=$2
      shift
      ;;
    -factory)
      CheckFile "$2"
      FACTORY_FILE=$2
      FLASH_FACTORY=1	# implied option: -ff
      shift
      ;;
    -factory-url)
      FACTORY_URL=$2
      FLASH_FACTORY=1	# implied option: -ff
      shift
      ;;
    -boot4perm)
      CheckFile "$2"
      BOOT4PERM_IMG=$2
      shift
      ;;
    *)
      Usage 1
    esac
    shift
  done

  # set PATH
  export PATH=$PATH:$SDK_PATH/platform-tools

  # cross-arguments checks
  if [ -n "$BOOT4PERM_IMG" ]; then
    if [ "$FLASH_ROM" -eq 1 ] && CheckFile "$ROM_DIR/boot.img"; then
      DO_BOOT4PERM=1
    else
      Error "the '-boot4perm' option requires either the '-rom' or '-fr' option is also specified and the file boot.img exists in the rom directory"
    fi
  fi
}

Boot2Fastboot() {
  echo ">> Please make sure that the device has been unlocked and boot into the fastboot mode."
  echo "   0. Make sure the Nexus phone is the only device connected to the PC"
  echo "   1. Enter 'Developer Options' in Settings"
  echo "   2. Ensure the 'OEM unlock' option has been checked"
  echo "   3. Ensure the 'USB debug' option has been checked"
  echo "   4. Use 'adb reboot bootloader', or Power off then press the key combo 'volume-down & power'"
  ToConfirm

  printf ">> Try to boot the device into the fastboot mode and unlock if it's not done..."

  while true; do
    local mode=`GetMode`
    printf "[$mode]"
    case $mode in
    fastboot)
      break
      ;;
    device|recovery)
      adb reboot bootloader
      WaitMode "fastboot" 30
      ;;
    *)
      ReloadUSB
      sleep 1
      #false
      ;;
    esac
  done
  fastboot oem unlock > /dev/null 2>&1 || : # continue script on failure

  echo " done"
}

FlashFactoryImage() {
  # check the decompressed directory
  if ! CheckDir "$FACTORY_DIR" probe-only -o ! -x "$FACTORY_DIR"/flash-all.sh; then
    # check the local file
    if ! CheckFile "$FACTORY_FILE" probe-only; then
      # download from URL
      if ! wget "$FACTORY_URL"; then
        Error newline "failed to download the factory image '$_url'! Please check if the URL is correct."
      fi
    fi
    # decompress the file
    if ! tar zxvf $FACTORY_FILE; then
      Error newline "failed to extract factory data from '$FACTORY_FILE'."
    fi
  fi

  echo ">> Start to flash the factory image from '$FACTORY_DIR'"

  # flash factory image
  pushd $FACTORY_DIR > /dev/null
  ./flash-all.sh
  popd > /dev/null

  echo "done"

  echo ">> The device has been recovered with the factory image '$FACTORY_DIR'"
  # echo ">> If you want to install Open Gapps, please run $(basename $0) again with -f"
  exit 0
}

FlashImages() {
  if [ -n "$ROM_ZIP" ] && CheckFile "$ROM_ZIP"; then
    printf ">> Flash ROM zip images '$ROM_ZIP' " && ToConfirm
    fastboot -w update "$ROM_ZIP"

    echo ">> Get ready to reboot the device"
    if WaitMode "device$" 30; then
      adb reboot bootloader
    else
      Error newline "unable to reboot the device into the fastboot mode, perhapse the USB debug mode is disabled in the flashed image."
    fi

    return 0
  else
    CheckDir "$ROM_DIR"
    pushd $ROM_DIR > /dev/null

    echo ">> Flash ROM images in '$ROM_DIR' " && ToConfirm
    CheckFile boot.img probe-only     && fastboot flash boot boot.img
    CheckFile recovery.img probe-only && fastboot flash recovery recovery.img
    CheckFile system.img probe-only   && fastboot flash system system.img
    CheckFile vendor.img probe-only   && fastboot flash vendor vendor.img
    CheckFile userdata.img probe-only && fastboot flash userdata userdata.img || fastboot erase data
    CheckFile cache.img probe-only    && fastboot flash cache cache.img || fastboot erase cache
    echo "done"

    popd > /dev/null
  fi
}

Boot2Recovery() {
  # download the twrp recovery image
  GetFile "$RECOVERY_URL" "$RECOVERY_FILE"

  echo ">> Boot into the recovery mode with '$RECOVERY_FILE' " && ToConfirm

  # use fastboot to boo into the recovery mode
  while true;
  do
    case `GetMode` in
    fastboot)
      fastboot boot $RECOVERY_FILE
      WaitMode "recovery" 30
      break
      ;;
    device|recovery)
      adb reboot bootloader
      WaitMode "fastboot" 30
      ;;
    esac
  done
  echo "done"
}

InstallOpenGapps() {
  # download the open gapps file
  GetFile "$GAPPS_URL" "$GAPPS_FILE"

  # wait for the sideload mode until timeout
  printf ">> Wait for ADB on the device to get ready"

  # Check to see if the device is able to enter the sideload mode
  while true; do
    case `GetMode` in
    recovery)
      echo ">> Now wipe the device"
      adb shell twrp wipe cache
      WaitMode "recovery" 30
      sleep 2
      echo ">> Upload Gapps into the device"
      adb push "$GAPPS_FILE" "/cache/$GAPPS_FILE"
      echo ">> Install Gapps"
      adb shell twrp install "/cache/$GAPPS_FILE"
      # wipe cache again
      WaitMode "recovery" 30
      sleep 2
      adb shell twrp wipe cache
      WaitMode "recovery" 30
      sleep 2
      echo "done"
      break;
      ;;
    fastboot)
      echo fastboot; sleep 1
      ;;
    device)
      echo 'device'; sleep 1
      ;;
    esac
  done

  WaitMode "recovery" 10 || Error "problem occurred while doing sideload, you may need to reboot the device and restart the whole process."
}

GrantPermissions() {
  local _failed=0

  echo ">> Prepare to grant permissions to GApps " && ToConfirm

  if [ "$DO_BOOT4PERM" -eq 1 ]; then
    echo ">> Replace the boot image with the specified '$BOOT4PERM_IMG'"
    adb reboot bootloader
    fastboot flash boot "$BOOT4PERM_IMG"
    fastboot reboot
  else
    echo ">> Reboot into Android"
    adb reboot
  fi

  if Wait4App2Appear gms 60; then
    echo ">> Start to grant permissions"
    adb shell pm grant com.google.android.setupwizard android.permission.READ_PHONE_STATE
    adb shell pm grant com.google.android.gms android.permission.ACCESS_FINE_LOCATION
    adb shell pm grant com.google.android.gms android.permission.ACCESS_COARSE_LOCATION
    adb shell pm grant com.google.android.gms android.permission.INTERACT_ACROSS_USERS
    adb shell pm grant com.google.android.gms android.permission.BODY_SENSORS
    adb shell pm grant com.google.android.gms android.permission.READ_SMS
    adb shell pm grant com.google.android.gms android.permission.RECEIVE_MMS
    adb shell pm grant com.google.android.gms android.permission.READ_EXTERNAL_STORAGE
    adb shell pm grant com.google.android.gms android.permission.WRITE_EXTERNAL_STORAGE
    adb shell pm grant com.google.android.gms android.permission.READ_CALENDAR
    adb shell pm grant com.google.android.gms android.permission.CAMERA
    adb shell pm grant com.google.android.gms android.permission.READ_CONTACTS
    adb shell pm grant com.google.android.gms android.permission.WRITE_CONTACTS
    adb shell pm grant com.google.android.gms android.permission.GET_ACCOUNTS
    adb shell pm grant com.google.android.gms android.permission.RECORD_AUDIO
    adb shell pm grant com.google.android.gms android.permission.READ_PHONE_STATE
    adb shell pm grant com.google.android.gms android.permission.CALL_PHONE
    adb shell pm grant com.google.android.gms android.permission.READ_CALL_LOG
    adb shell pm grant com.google.android.gms android.permission.PROCESS_OUTGOING_CALLS
    adb shell pm grant com.google.android.gms android.permission.SEND_SMS
    adb shell pm grant com.google.android.gms android.permission.RECEIVE_SMS
    echo "done"
  else
    _failed=1
  fi

  if [ "$DO_BOOT4PERM" -eq 1 ]; then
    echo ">> Restore the original flashed boot image"
    adb reboot bootloader
    fastboot flash boot "$ROM_DIR/boot.img"
    fastboot reboot
  fi

  if [ "$_failed" -eq 1 ]; then
    Error "failed to grant permissions for Open Gapps, please do that manually (for Google Play Service, specifically)"
  fi
}

# ----------------------
# Main Program goes here
# ----------------------

# Check to see if tools (adb, fastboot) can be found
CheckTools

# Check parameters
CheckParams "$@"

# Try to boot the device into fastboot mode
Boot2Fastboot

# Check to see if the factory image needs to be flashed first
[ "$FLASH_FACTORY" -eq 1 ] && FlashFactoryImage

# flash aosp image
[ "$FLASH_ROM" -eq 1 ] && FlashImages

# boot in to recovery mode
Boot2Recovery

# sideload gapp ota
InstallOpenGapps

# grant permissions to setup wizard and gms
GrantPermissions
