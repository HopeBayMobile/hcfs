#!/bin/bash

seed_file="delta.seed"
#seed_file="delta_gw_iii.seed"
#seed_file="delta_gw_fix.seed"
dest_dir="dest_ubuntu/source_ubuntu"
txt_file="$dest_dir/isolinux/txt.cfg"
isolinux_file="$dest_dir/isolinux/isolinux.cfg"
iso_file="delta_gateway_install.iso"
gateway_package="gateway_package/gateway_install_pkg*.tar"
gateway_version="1.0.16_20121003a"
script_dir="script"

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

if [ -n "$1" ]; then
    date_str=$1
    iso_file="delta_gateway_install_$1.iso"
else
    iso_file="delta_gateway_install_$gateway_version.iso"
fi

# generate README file
cat<<EOF > README
This iso will install ubuntu server 12.04 automatically, and will setup and install the following item and package.
1. DCloudGateway version $gateway_version
2. set /etc/grub.d/00header timeout=-1 to timeout=2
3. have raid1 auto rebuild script
EOF


cp README $dest_dir/
cp $seed_file $dest_dir/preseed/
cp $gateway_package $dest_dir/preseed/
cp $script_dir/* $dest_dir/preseed/
preseed_md5sum=`md5sum $seed_file | cut -d " " -f 1`
sed -i "5c \  append preseed/file=/cdrom/preseed/$seed_file preseed/file/checksum=$preseed_md5sum auto=true priority=critical initrd=/install/initrd.gz ramdisk_size=16384 root=/dev/ram rw quiet --" $txt_file
sed -i "5c timeout 10" $isolinux_file
chmod -R 777 $dest_dir

mkisofs -r -V "Delta Install CD" \
            -cache-inodes \
            -J -l -b isolinux/isolinux.bin \
            -c isolinux/boot.cat -no-emul-boot \
            -boot-load-size 4 -boot-info-table \
            -o $iso_file $dest_dir

