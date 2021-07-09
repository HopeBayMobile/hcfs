#!/bin/bash
##
## Copyright (c) 2021 HopeBayTech.
##
## This file is part of Tera.
## See https://github.com/HopeBayMobile for further info.
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##


###notation
##TAG
deconf() {
adb push hcfsconf /data/
adb shell chmod 777 /data/hcfsconf
adb shell /data/hcfsconf dec /data/hcfs.conf /tmp/hcfs.conf.dec
adb pull /tmp/hcfs.conf.dec /tmp/hcfs.conf.dec
}

disableAccount() {
curl -u hopebayadmin:4nXiC9X6 -i -X PUT -H "Content-Type: application/json"     http://10.10.99.120:5000/api/account/$1 -d '{"enable": false}'
}

enableAccount() {
curl -i -X PUT -H "Content-Type: application/json"  -u hopebayadmin:4nXiC9X6   http://10.10.99.120:5000/api/account/$1 -d  '{"enable":true}'
}

checkBackendStat() {
adb shell HCFSvol cloudstat
}

checkSyncStat() {
adb shell HCFSvol getsyncswitch
}

cleanLog() {
adb shell 'echo > /data/hcfs_android_log'
}
offline() {
adb shell iptables -I INPUT -p all -s 61.219.202.83 -d 0.0.0.0/0 -j DROP
adb shell iptables -I OUTPUT -p all -s 0.0.0.0/0 -d 61.219.202.83 -j DROP
#adb shell svc wifi disable
}

online() {
adb shell iptables -D INPUT 1
adb shell iptables -D OUTPUT 1
#adb shell svc wifi enable
}

access() {
adb shell mkdir /sdcard/testDir
adb shell touch /sdcard/testDir/access
adb shell rm -rf /sdcard/testDir
#adb shell input keyevent "KEYCODE_HOME"
#adb shell input tap 900 1600 
#adb shell input swipe 538 1621 538 1621 100
}

setup() {
cleanLog
adb shell HCFSvol changelog 5
}

teardown() {
adb shell HCFSvol changelog 4
cleanLog
}

backendOfflineStart() {
count=0
until [ $count -gt 0 ] && grep "wait " /tmp/log.tmp > /dev/null > /dev/null 2>&1
do
pullLog
count=`expr $count + 1`
sleep 1
#echo $count
start_line=$(grep "backend \[offl" /tmp/log.tmp -n |awk -F":" '{print $1}')
grep "backend \[offl" /tmp/log.tmp > /dev/null > /dev/null 2>&1
done
return $start_line
}


pullLog() {
adb pull /data/hcfs_android_log /tmp/log.tmp > /dev/null 2>&1

}

testBackendMonitorOffline() {
echo "###backend monitor -- backend offline"
echo "##HCFS_ANDROID_870_01"
setup
offline
backendOfflineStart
start_line=$?
echo "start time"
adb shell date +%Y%m%d_%H%M%S 
echo $start_line
sleep 1022
pullLog
echo "end time"
adb shell date +%Y%m%d_%H%M%S 
wait_count=$(sed -n "$start_line,$"p /tmp/log.tmp | grep 'Monitor' |grep 'wait'|wc -l)
echo $wait_count
online
teardown
if [ $wait_count -gt 9 ];then
    echo "[O]"
    return 0
else
    echo "[X]"
    return 1
fi
}


testBackendMonitorOnline() {
echo "###backend monitor -- backend from offline to online"
echo "##HCFS_ANDROID_870_02"
setup
offline
backendOfflineStart
start_line=$?
echo $start_line
sleep 10
online
sleep 512
pullLog
#wait_count=$(sed -n "$start_line,$"p /tmp/log.tmp | grep 'Monitor' |grep 'online')
online
teardown
if [ $(adb shell 'HCFSvol cloudstat | grep ONLINE > /dev/null; echo $?') != 0 ];then
    echo "[O]"
    return 0
else
    echo "[X]"
    return 1
fi
}

testBackendMonitorDataAccess() {
###backend monitor -- data access
##HCFS_ANDROID_870_03
setup
offline
if [ $(adb shell 'HCFSvol cloudstat | grep OFFLINE > /dev/null; echo $?') != 0 ];then
    access
    if [ $(adb shell 'HCFSvol cloudstat | grep ONLINE > /dev/null; echo $?') != 0 ];then   
        echo "[O]"
        online
        teardown
        return 0
    else
        echo "[X]"
        online
        teardown
        return 2
    fi
else
    echo "[X]"
    online
    teardown
    return 1
fi
}

testBackendMonitorOfflineLongTime() {
echo "###backend monitor -- backend offline long time"
echo "##HCFS_ANDROID_870_04"
setup
offline
backendOfflineStart
start_line=$?
echo $start_line
sleep 3600
online
sleep 512
pullLog
#wait_count=$(sed -n "$start_line,$"p /tmp/log.tmp | grep 'Monitor' |grep 'online')
online
teardown
if [ $(adb shell 'HCFSvol cloudstat | grep ONLINE > /dev/null; echo $?') != 0 ];then
    echo "[O]"
    return 0
else
    echo "[X]"
    return 1
fi
}


testBackendMonitorAccountDisable() {
###backend account disable
##HCFS_ANDROID_870_05
setup
deconf
SWIFT_ACCOUNT=$(grep 'SWIFT_ACCOUNT' /tmp/hcfs.conf.dec |awk -F ' = ' '{print $2}')
disableAccount $SWIFT_ACCOUNT
sleep 5
access
teardown
if [ $(adb shell 'HCFSvol cloudstat | grep OFFLINE > /dev/null; echo $?') != 0 ];then
    echo "[O]"
    enableAccount $SWIFT_ACCOUNT
    return 0
else
    echo "[X]"
    enableAccount $SWIFT_ACCOUNT
    return 1
fi

}

####Main####


    case $1 in
        1) 
            testBackendMonitorOffline
            ;;
        2)         
            testBackendMonitorOnline
            ;;
        3)         
            testBackendMonitorDataAccess
            ;;
        4)         
            testBackendMonitorOfflineLongTime
            ;;
        5)         
            testBackendMonitorAccountDisable
            ;;
        *)
            ;;
    esac

#testBackendMonitorOffline
#testBackendMonitorOnline
