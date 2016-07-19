#!/bin/bash

###notation
##TAG

testOpenFileWriteSameBlockSync() {
###    open file and write the same block – sync with create system call
##HCFS_ANDROID_000_02
EXEC=US000_02
cd TestCases/HCFSANDROID/test_scripts/US000_02/
adb push $EXEC /data/local/tmp/
adb shell /data/local/tmp/$EXEC
./doCheck_logined
if [ $? != 0 ];then
    echo "[O]"
    return 0
else
    echo "[X]"
    return 1
fi
}

testOpenFileWriteSameBlockSync2() {
###open file and write the same block – sync with open system call
##HCFS_ANDROID_000_02
EXEC=US000_02_OPEN
cd TestCases/HCFSANDROID/test_scripts/US000_02_OPEN/
adb push $EXEC /data/local/tmp/
adb shell /data/local/tmp/$EXEC
./doCheck_logined
if [ $? != 0 ];then
    echo "[O]"
    return 0
else
    echo "[X]"
    return 1
fi
}


####Main####

   case $1 in
        1)             
            ;;
        2)     
            testOpenFileWriteSameBlockSync
            ;;
        3)         
            ;;
        4)
            testOpenFileWriteSameBlockSync      
            ;;
        *)
            ;;
    esac






