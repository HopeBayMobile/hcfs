#!/system/bin/sh

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
    #/system/bin/hcfs -oallow_other,big_writes &
    #/system/bin/hcfs -oallow_other,big_writes &
    /system/bin/hcfs -oallow_other,big_writes,subtype=hcfs,fsname=/dev/fuse &
    #/system/bin/hcfs -oallow_other,big_writes,subtype=aaa.bbb &

    while [ ! -e /dev/shm/hcfs_reporter ]; do sleep 0.1; done

    /system/bin/HCFSvol create hcfs_data internal
    /system/bin/HCFSvol mount hcfs_data /data/data

    chown system:system /data/data
    chmod 771 /data/data

    #mkdir /data/app
    #/system/bin/HCFSvol create hcfs_app internal
    #/system/bin/HCFSvol mount hcfs_app /data/app

    #chown system:system /data/app
    #chmod 771 /data/app

    #start hcfs_api
}

################################################
#
# MAIN
#
################################################

[ -n "`grep ${MOUNTPT} /proc/mounts | grep ext4`" ] && init_hcfs

touch /tmp/hcfs_ready
