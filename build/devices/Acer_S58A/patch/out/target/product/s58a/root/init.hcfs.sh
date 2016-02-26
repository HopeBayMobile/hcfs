#!/system/bin/sh

mkdir /data/data
/system/bin/HCFSvol create hcfs_data internal
/system/bin/HCFSvol mount hcfs_data /data/data

chown system:system /data/data
chmod 771 /data/data

mkdir /data/app
/system/bin/HCFSvol create hcfs_app internal
/system/bin/HCFSvol mount hcfs_app /data/app

chown system:system /data/app
chmod 771 /data/app

touch /tmp/hcfs_ready
#mkdir /data/data/media
#chmod 770 /data/data/media
#chown media:media /data/data/media 
