#!/system/bin/sh

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
