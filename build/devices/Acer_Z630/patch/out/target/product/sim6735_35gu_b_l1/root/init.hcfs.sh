#!/system/bin/sh
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

output=`HCFSvol list | grep hcfs_test`
if [ "$output" = "" ]; then
    /system/bin/HCFSvol create hcfs_test external
fi

/system/bin/HCFSvol mount hcfs_test /storage/sdcard0

if [ -e "/data/data_bak" ]; then
    rm -rf /data/data
    mkdir /data/data

    output=`HCFSvol list | grep hcfs_data`
    if [ "$output" = "" ]; then
        /system/bin/HCFSvol create hcfs_data internal
    fi

    /system/bin/HCFSvol mount hcfs_data /data/data

    chown system:system /data/data
    chmod 771 /data/data

else
    mv /data/data /data/data_bak
    mkdir /data/data

    output=`HCFSvol list | grep hcfs_data`
    if [ "$output" = "" ]; then
        /system/bin/HCFSvol create hcfs_data internal
    fi

    /system/bin/HCFSvol mount hcfs_data /data/data

    chown system:system /data/data
    chmod 771 /data/data

    cp -aNp /data/data_bak/* /data/data/
fi

if [ -e "/data/app_bak" ]; then
    rm -rf /data/app
    mkdir /data/app

    output=`HCFSvol list | grep hcfs_app`
    if [ "$output" = "" ]; then
        /system/bin/HCFSvol create hcfs_app internal
    fi

    /system/bin/HCFSvol mount hcfs_app /data/app
else
    mv /data/app /data/app_bak
    mkdir /data/app

    output=`HCFSvol list | grep hcfs_app`
    if [ "$output" = "" ]; then
        /system/bin/HCFSvol create hcfs_app internal
    fi

    /system/bin/HCFSvol mount hcfs_app /data/app

    cp -aNp /data/app_bak/* /data/app/
fi

chown system:system /data/app
chmod 771 /data/app

chmod 707 /data/data/com.hopebaytech.hcfsmgmt

/system/bin/socket_serv &

sleep 5

rm -rf /storage/sdcard0/mtklog
