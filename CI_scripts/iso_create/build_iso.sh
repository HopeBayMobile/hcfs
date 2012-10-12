#!/bin/bash

#seed_name="delta.seed"
#seed_name="delta_test.seed"
seed_name="delta_only_os.seed"
#seed_name="delta_only_os_swap.seed"
seed_dir="seed"
dest_dir="dest_ubuntu/source_ubuntu"
txt_file="$dest_dir/isolinux/txt.cfg"
isolinux_file="$dest_dir/isolinux/isolinux.cfg"
iso_file="gateway_install.iso"
gateway_dir="gateway_package"
gateway_name="gateway_install_pkg*.tar"
iso_version="only_os"
script_dir="script"

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

if [ -n "$1" ]; then
    iso_version="$1";
    gateway_name=`ls $gateway_dir | grep "$1"`
    if [ -z gateway_name ]; then
        echo "gateway package is not exist!!"
	exit 0
    fi
    gateway_package="$gateway_dir/$gateway_name"
    cp $gateway_package  $dest_dir/preseed/
    seed_name="delta.seed"   
fi
iso_file="gateway_install_$iso_version.iso"

mkdir -p $seed_dir
mkdir -p $gateway_dir
mkdir -p $script_dir

# generate README file
if [ $iso_version = "only_os" ]; then
cat<<EOF > README
This iso will install ubuntu server 12.04 automatically, and will setup and install the following item and package.
1. No DCloudGateway
2. set /etc/grub.d/00header timeout=-1 to timeout=2
3. have raid1 auto rebuild script
EOF
else
cat<<EOF > README
This iso will install ubuntu server 12.04 automatically, and will setup and install the following item and package.
1. DCloudGateway version $iso_version
2. set /etc/grub.d/00header timeout=-1 to timeout=2
3. have raid1 auto rebuild script
EOF
fi

seed_file="$seed_dir/$seed_name"
cp README $dest_dir/
cp $seed_file $dest_dir/preseed/
cp $script_dir/* $dest_dir/preseed/
preseed_md5sum=`md5sum $seed_file | cut -d " " -f 1`
sed -i "5c \  append preseed/file=/cdrom/preseed/$seed_name preseed/file/checksum=$preseed_md5sum auto=true priority=critical initrd=/install/initrd.gz ramdisk_size=16384 root=/dev/ram rw quiet --" $txt_file
sed -i "5c timeout 10" $isolinux_file
chmod -R 777 $dest_dir

mkisofs -r -V "Delta Install CD" \
            -cache-inodes \
            -J -l -b isolinux/isolinux.bin \
            -c isolinux/boot.cat -no-emul-boot \
            -boot-load-size 4 -boot-info-table \
            -o $iso_file $dest_dir

