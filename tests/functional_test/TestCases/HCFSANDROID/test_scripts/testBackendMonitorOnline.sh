#!/bin/bash

###notation
##TAG

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
offline
}

teardown() {
online
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
#disableAccount() {
#
#}

pullLog() {
adb pull /data/hcfs_android_log /tmp/log.tmp > /dev/null 2>&1

}

testBackendMonitorOffline() {
echo "###backend monitor -- backend offline"
echo "##HCFS_ANDROID_870_01"
setup
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
backendOfflineStart
start_line=$?
echo $start_line
sleep 10
online
sleep 512
pullLog
#wait_count=$(sed -n "$start_line,$"p /tmp/log.tmp | grep 'Monitor' |grep 'online')
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
if [ $(adb shell 'HCFSvol cloudstat | grep OFFLINE > /dev/null; echo $?') != 0 ];then
    access
    if [ $(adb shell 'HCFSvol cloudstat | grep ONLINE > /dev/null; echo $?') != 0 ];then   
        echo "[O]"
        teardown
        return 0
    else
        echo "[X]"
        teardown
        return 2
    fi
else
    echo "[X]"
    teardown
    return 1
fi
}

#testBackendMonitorOffline2() {
###backend offline long time
##HCFS_ANDROID_870_04
#}


#testBackendMonitorAccountDisable() {
###backend account disable
##HCFS_ANDROID_870_05
#}

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
        *)
            ;;
    esac

#testBackendMonitorOffline
#testBackendMonitorOnline
