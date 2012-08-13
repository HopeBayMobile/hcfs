#!/bin/bash	
# This script initializes two local HDDs, i.e., to build a RAID 1 storage.
# It will check whether or not the HDDs have been inited. (by checking a token
# which is created after init. process)
# If the token exists, do nothing. 
# Otherwise, two disks will be re-partioned into RAID type, and then build a new
# new file system on it, and mount to /storage
#
# To install the script, please add the follwoing line into /etc/rc.local
#
# /root/build_raid1.sh > /root/log.txt
#
# author: Jashing
#
# v0.1, May 15, 2012
# v0.2, July 05, 2012 by Yen
# v0.3, July 20, 2012 by wthung

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# install mdadm package
echo debconf postfix/main_mailer_type select No configuration | /usr/bin/debconf-set-selections
apt-get -y --force-yes install mdadm

# this token will be written if the raid is built successfuly
SAFE_TOKEN="/storage/.init_raid_ok"

# if the token exists
if [ -e $SAFE_TOKEN ]
then
    # just finish, do nothing.
        echo "RAID1 inited"
else
    # the disks have not inited. yet.
    # Confirm with the operator by 10 seconds timeout
    fdisk -l 
    echo "Do you wish to format sdb and sdc and build RAID1 on them? [Y/n]"
    read -t 10 input
    if [ $? -ne 0 ] || [ "$input" == "y" ] || [ "$input" == "Y" ]
    then
        if [ ! -e "/storage" ]; then
             # make mount point
             mkdir /storage
        fi
    
        # assume two hdds are /dev/sdb /dev/sdc
        # Notice that all data will lose
        dd if=/dev/zero of=/dev/sdb bs=512 count=1
        parted -s /dev/sdb mklabel msdos
        parted -s /dev/sdb mkpart primary 0 100%
        parted -s /dev/sdb set 1 raid on
        dd if=/dev/zero of=/dev/sdc bs=512 count=1
        parted -s /dev/sdc mklabel msdos
        parted -s /dev/sdc mkpart primary 0 100%
        parted -s /dev/sdc set 1 raid on
    
        # re-probe partition
        sleep 1
        partprobe
    
        # build raid 1
        sleep 1
        mdadm --create /dev/md127 --run --level=1 --raid-devices=2 /dev/sdb1 /dev/sdc1
    
        # make ext4 file system on it
        sleep 1
        mkfs.ext4 /dev/md127
    
        # mount the raid device to the mount point
        sleep 1
        mount /dev/md127 /storage
    
        # write some conf. into system, 
        # such that the raid will start and be mounted automatically,
         
        sleep 1
        echo "/dev/md127	/storage	ext4	defaults 0 0" >> /etc/fstab
        mdadm --detail --scan >> /etc/mdadm/mdadm.conf
    
        # write a token for checking whether the raid has been inited.
        touch $SAFE_TOKEN
    
        #initialize necessary files/directories
    
        dd if=/dev/zero of=/storage/swapfile bs=1024 count=20480000
        mkswap /storage/swapfile
        swapon /storage/swapfile
        mkdir /storage/log
        mkdir /storage/s3ql
        mkdir /storage/log/apache2
        # add symbolic link
        rm -r /var/log 
        rm -r /root/.s3ql
        ln -s /storage/log /var/log
        ln -s /storage/s3ql /root/.s3ql
        # remove grub recordfail
        sudo grub-editenv - unset recordfail
        sudo update-grub2
    else
        exit
    fi
fi 

## add a script in /root/ to check raid1 every time
cat >/root/build_raid1.sh <<EOF
    # this token will be written if the raid is built successfuly
    SAFE_TOKEN="/storage/.init_raid_ok"

    # if the token exists
    if [ -e $SAFE_TOKEN ]
    then
        # just finish, do nothing.
            echo "RAID1 inited"
    else
            if [ ! -e "/storage" ]; then
                 # make mount point
                 mkdir /storage
            fi
        
            # assume two hdds are /dev/sdb /dev/sdc
            # Notice that all data will lose
            dd if=/dev/zero of=/dev/sdb bs=512 count=1
            parted -s /dev/sdb mklabel msdos
            parted -s /dev/sdb mkpart primary 0 100%
            parted -s /dev/sdb set 1 raid on
            dd if=/dev/zero of=/dev/sdc bs=512 count=1
            parted -s /dev/sdc mklabel msdos
            parted -s /dev/sdc mkpart primary 0 100%
            parted -s /dev/sdc set 1 raid on
        
            # re-probe partition
            sleep 1
            partprobe
        
            # build raid 1
            sleep 1
            mdadm --create /dev/md127 --run --level=1 --raid-devices=2 /dev/sdb1 /dev/sdc1
        
            # make ext4 file system on it
            sleep 1
            mkfs.ext4 /dev/md127
        
            # mount the raid device to the mount point
            sleep 1
            mount /dev/md127 /storage
        
            # write some conf. into system, 
            # such that the raid will start and be mounted automatically,
             
            sleep 1
            echo "/dev/md127	/storage	ext4	defaults 0 0" >> /etc/fstab
            mdadm --detail --scan >> /etc/mdadm/mdadm.conf
        
            # write a token for checking whether the raid has been inited.
            touch $SAFE_TOKEN
        
            #initialize necessary files/directories
        
            dd if=/dev/zero of=/storage/swapfile bs=1024 count=20480000
            mkswap /storage/swapfile
            swapon /storage/swapfile
            mkdir /storage/log
            mkdir /storage/s3ql
            mkdir /storage/log/apache2
            # add symbolic link
            rm -r /var/log 
            rm -r /root/.s3ql
            ln -s /storage/log /var/log
            ln -s /storage/s3ql /root/.s3ql
            # remove grub recordfail
            sudo grub-editenv - unset recordfail
            sudo update-grub2
    fi
EOF

## append build_raid1.sh to /etc/rc.local
cat >/etc/init.d/check_raid1 <<EOF
/root/build_raid1.sh
EOF
chmod 777 /etc/init.d/check_raid1
cp -rs /etc/init.d/check_raid1 /etc/rc2.d/S10check_raid1
