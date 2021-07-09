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






