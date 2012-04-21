#!/bin/bash
# This script is to recover the system volume server after a failed deployment.
# This script is invoked by ./SystemVolumeDeploy.sh and also must be executed 
# in root privilege.
# History:
# 2012/03/21 CW First release


echo -e "\n[`date`]: Errors occured in the deploy process. The recovery process is going to start."


# remove all installed packages
echo -e "\n[`date`]: Remove all installed packages"
#/etc/init.d/glusterfs-server stop
#dpkg -r glusterfs-client glusterfs-common glusterfs-dbg glusterfs-examples glusterfs-server
#dpkg --purge glusterfs-common glusterfs-server
#rm -rf /etc/glusterd


# recover all modified configuration files
echo -e "\n[`date`]: Restore all modified configuration files"
mv /etc/hosts.backup /etc/hosts
mv /etc/fstab.backup /etc/fstab
mv /etc/resolv.conf.backup /etc/resolv.conf
mv /etc/rc.local.backup /etc/rc.local
mv /etc/network/interfaces.backup /etc/network/interfaces
/etc/init.d/networking restart


#echo -e "\n[`date`]: Restore /etc/hosts"
#touch /etc/hosts_tmp
#cat /etc/hosts | while read line
#do
#	if [ "$line" != "192.168.11.1 MANAGEMENT1" ] && [ "$line" != "192.168.11.2 MANAGEMENT2" ]; then
#		echo $line >> /etc/hosts_tmp
#	fi
#done
#mv /etc/hosts_tmp /etc/hosts


umount /export2

