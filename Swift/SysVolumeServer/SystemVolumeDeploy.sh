#!/bin/bash
# After installing Ubuntu 11.04 in /dev/sda and configuring the network, 
# the script is used to create the system volume. The system volume can be
# mounted with the nfs access protocol by other hosts.
# Note that the script must be executed in root privilege.
# The nfs export mounting entry is xxx.xxx.xxx.xxx/SystemVolume.
# History:
# 2012/03/21 CW First release


# check the host name
echo -e "\n[`date`]: Check whether the host name is MANAGEMENT3"
HostName=`hostname`
if [ "$HostName" != "MANAGEMENT3" ]; then
	echo "Please modify the host name to MANAGEMENT3."
	exit 1
fi


# backup the related configuration files
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


# add hosts into /etc/hosts
echo -e "\n[`date`]: Add Management 1, 2, and 3 into /etc/hosts"
sudo echo "192.168.11.1 MANAGEMENT1" >> /etc/hosts
sudo echo "192.168.11.2 MANAGEMENT2" >> /etc/hosts
sudo echo "192.168.11.3 MANAGEMENT3" >> /etc/hosts


# set up the network configuration
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


# check the status of network configuration
echo -e "\n[`date`]: Check the network configuration"
IP=""
IP=`ifconfig eth1 | grep 'inet addr:' | cut -d: -f2 | awk '{print $1}'`
if [ "$IP" = "" ]; then
	./RecoverDeploy.sh
	echo "Please set the network configuration of eth1."
	exit 1
fi


# install all required packages
echo -e "\n[`date`]: Install all required packages"
dpkg -i ./deb_source/*.deb


# check the status of all disks
echo -e "\n[`date`]: Check the existence of all disks"
for device in b c d e f
if [ ! -e /dev/sd$device ]; then
	./RecoverDeploy.sh
	echo "Device /dev/sdb does not exist! Please check the status of /dev/sd$device."
else
	mkdir -p /export_sd$device
	mkfs -t ext4 /dev/sd$device << EOF
y
EOF
	mount /dev/sd$device /export_sd$device
	echo "/dev/sd$device /export_sd$device ext4 defaults 1 2" >> /etc/fstab
fi


# create the system volume
echo -e "\n[`date`]: Create the system volume"
# To create the system volume


# modify /etc/rc.local
echo -e "\n[`date`]: Modify /etc/rc.local"
sudo cat >/etc/rc.local << EOF
#!/bin/bash
#sudo /etc/init.d/glusterfs-server restart
#sudo gluster volume start SystemVolume
exit 0
EOF


# modify the IP of the name server
echo -e "\n[`date`]: Modify the IP of the name server in /etc/resolv.conf"
sudo cat >/etc/resolv.conf << EOF
nameserver 192.168.11.1
EOF


# try to test the nfs mount operation
sleep 10
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


# display the information of the mounting entry 
echo -e "\n[`date`]: SystemVolume is ready for use!"
echo "The mount point is $IP:/SystemVolume"
echo "The nfs mounting command is: mount -t nfs -o vers=3,nolock $IP:/SystemVolume /mnt."

