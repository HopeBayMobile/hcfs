#!/system/bin/sh

MOUNTPT=/data

init_hcfs() {
    mkdir /data/data
    mkdir /data/hcfs
    mkdir /data/hcfs/metastorage
    mkdir /data/hcfs/blockstorage

    /system/bin/hcfsconf enc ${HCFSSRC} ${HCFSCONF}

    while [ ! -e ${HCFSCONF} ]; do sleep 0.1; done

    /system/bin/hcfs -oallow_other,big_writes &

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
}

################################################
#
# MAIN
#
################################################

[ -n "`grep ${MOUNTPT} /proc/mounts`" ] && init_hcfs

touch /tmp/hcfs_ready
