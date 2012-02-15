#!/bin/bash
# This script is placed on rc.local after first executing
# ./SystemVolumeDeploy.sh.
# After rebooting system volume server, 
# the script is executed automatically to mount GlusterFS 
# volume on /SystemVolume (nfs export directory), and to
# restart the service of nfs-kernel-server.
# History:
# 2012/02/15 CW First release

sudo /etc/init.d/glusterfs-server restart
sudo mkdir -p /SystemVolume
IP=""
IP=`ifconfig eth1 | grep 'inet addr:' | cut -d: -f2 | awk '{print $1}'`
sudo gluster volume start SystemVolume
sudo mount -t glusterfs $IP:/SystemVolume /SystemVolume
sleep 5
sudo /etc/init.d/nfs-kernel-server restart
echo "The mount point is $IP:/SystemVolume"
