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

MOUNTPT=/data
HCFSSRC=/etc/hcfs.conf
HCFSCONF=/data/hcfs.conf

init_hcfs() {
    mkdir /data/data
    mkdir /data/hcfs
    mkdir /data/hcfs/metastorage
    mkdir /data/hcfs/blockstorage

    /system/bin/hcfsconf enc ${HCFSSRC} ${HCFSCONF}

    while [ ! -e ${HCFSCONF} ]; do sleep 0.1; done

    #start hcfs
    /system/bin/hcfs -oallow_other,big_writes,subtype=hcfs,fsname=/dev/fuse &

    while [ ! -e /dev/shm/hcfs_reporter ]; do sleep 0.1; done

    /system/bin/HCFSvol create hcfs_data internal
    /system/bin/HCFSvol mount hcfs_data /data/data
    
    chown system:system /data/data
    chmod 771 /data/data
    
    mkdir /data/app
    /system/bin/HCFSvol create hcfs_app internal
    /system/bin/HCFSvol mount hcfs_app /data/app
    
    chown system:system /data/app
    chmod 771 /data/app
    
    #start hcfs_api
}

################################################
#
# MAIN
#
################################################

[ -n "`grep ${MOUNTPT} /proc/mounts | grep ext4`" ] && init_hcfs

touch /tmp/hcfs_ready
