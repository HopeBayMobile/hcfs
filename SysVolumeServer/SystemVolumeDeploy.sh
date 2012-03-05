#!/bin/bash
# After installing Ubuntu 11.04 in /dev/sda and configuring the network, 
# the script is used to create the system volume. The system volume can be
# mounted with the nfs access protocol by other hosts.
# This system volume is a GlusterFS volume of replica 2. The nfs export mounting entry is 
# xxx.xxx.xxx.xxx/SystemVolume, and use the built-in nfs access protocol of GlusterFS.
# History:
# 2012/03/03 CW First release
# 2012/03/04 Modified by CW: add recovery operations after a failed deployment


echo -e "\n[`date`]: Check whether the host name is MANAGEMENT3"
HostName=`hostname`
if [ "$HostName" != "MANAGEMENT3" ]; then
	echo "Please modify the host name to MANAGEMENT3."
	exit 1
fi


echo -e "\n[`date`]: Backup the related files which would be modified later"
if [ ! -e /etc/fstab.backup ]; then
	cp /etc/fstab /etc/fstab.backup
fi
if [ ! -e /etc/hosts.backup ]; then
	cp /etc/hosts /etc/hosts.backup
fi
if [ ! -e /etc/resolv.conf.backup ]; then
	cp /etc/resolv.conf /etc/resolv.conf.backup
fi
if [ ! -e /etc/rc.local.backup ]; then
	cp /etc/rc.local /etc/rc.local.backup
fi
if [ ! -e /etc/network/interfaces.backup ]; then
	cp /etc/network/interfaces /etc/network/interfaces.backup
fi


echo -e "\n[`date`]: Add Management 1, 2, and 3 into /etc/hosts"
sudo echo "192.168.11.1 MANAGEMENT1" >> /etc/hosts
sudo echo "192.168.11.2 MANAGEMENT2" >> /etc/hosts
sudo echo "192.168.11.3 MANAGEMENT3" >> /etc/hosts


echo -e "\n[`date`]: Set up /etc/network/interfaces"
cat >/etc/network/interfaces << EOF
auto lo
iface lo inet loopback

auto eth1
iface eth1 inet static
address 192.168.11.3
netmask 255.255.255.0
EOF
/etc/init.d/networking restart


echo -e "\n[`date`]: Check the network configuration"
IP=""
IP=`ifconfig eth1 | grep 'inet addr:' | cut -d: -f2 | awk '{print $1}'`
if [ "$IP" = "" ]; then
	./RecoverDeploy.sh
	echo "Please set the network configuration of eth1."
	exit 1
fi


echo -e "\n[`date`]: Install GlusterFS and nfs-common"
sudo dpkg -i ./deb_source/glusterfs/*.deb
sudo dpkg -i ./deb_source/nfs-common/*.deb


echo -e "\n[`date`]: Check the existence of /dev/sdb"
if [ ! -e /dev/sdb ]; then
	./RecoverDeploy.sh
	echo "Device /dev/sdb does not exist! Please check the status of the hard disk."
	exit 1
fi


echo -e "\n[`date`]: Create the GlusterFS volume"
sudo /etc/init.d/glusterfs-server restart
sudo mkdir -p /export1
sudo mkdir -p /export2
sudo umount /dev/sdb
sudo umount /dev/sdb1
sudo umount /dev/sdb2
sudo umount /dev/sdb3
sudo umount /dev/sdb4
sudo mkfs -t ext4 /dev/sdb << EOF
y
EOF
sudo mount /dev/sdb /export2 
sudo gluster volume create SystemVolume replica 2 $IP:/export1 $IP:/export2
sudo gluster volume start SystemVolume


echo -e "\n[`date`]: Modify /etc/fstab and /etc/rc.local"
sudo echo "/dev/sdb /export2 ext4 defaults 1 2" >> /etc/fstab
sudo cat >/etc/rc.local << EOF
#!/bin/bash
sudo /etc/init.d/glusterfs-server restart
sudo gluster volume start SystemVolume
exit 0
EOF


echo -e "\n[`date`]: Modify the IP of the name server in /etc/resolv.conf"
sudo cat >/etc/resolv.conf << EOF
nameserver 192.168.11.1
EOF


echo -e "\n[`date`]: Self test of mounting the system volume with the nfs protocol"
sudo mount -t nfs -o vers=3,nolock $IP:/SystemVolume /mnt
MountCode=$?
if [ "$MountCode" != 0 ]; then
	./RecoverDeploy.sh
	echo "The system volume cannot be mounted with the nfs protocol!"
	exit 1
fi
sudo umount /mnt
UmountCode=$?
if [ "$UmountCode" != 0 ]; then
	./RecoverDeploy.sh
	echo "Fail to unmount the system volume!"
	exit 1
fi


echo -e "\n[`date`]: SystemVolume is ready for use!"
echo "The mount point is $IP:/SystemVolume"
echo "The nfs mounting command is: mount -t nfs -o vers=3,nolock $IP:/SystemVolume /mnt."

