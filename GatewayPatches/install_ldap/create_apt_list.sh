# cat create_apt_list.sh
#!/bin/bash

#INTERNET=no
# mount precise dvd iso
DVDISO=no
TARDEBFILE="./tardeb.tgz"
TARDEB=no
APTCACHEDIR=/var/cache/apt

if [ ! -d /mnt/iso ]; then
    sudo mkdir -p /mnt/iso
else
    sudo umount /mnt/iso
fi


if [ -f /iso/ubuntu-12.04-dvd-amd64.iso ]; then
    LOOPDEVICE=`sudo losetup -f`
    sudo mount -t iso9660 -o ro,loop=${LOOPDEVICE} /iso/ubuntu-12.04-dvd-amd64.iso /mnt/iso
else
    DVDISO=yes
fi

if [ -f ${TARDEBFILE} ]; then
    TARDEB=yes
    #-- Ovid Wu <ovid.wu@delta.com.tw> Mon, 06 Aug 2012 06:18:03 +0000
    # Note: here assume all debs are tarred in full directory path /var/cache/apt
    sudo tar zxvf ${TARDEBFILE} -C /
    sudo dpkg-scanpackages ${APTCACHEDIR} > ${APTCACHEDIR}/Packages
    sudo gzip ${APTCACHEDIR}/Packages
fi


#if [ ${INTERNET} = "yes" ]; then

if [ ${DVDISO} = yes ]; then
echo "Creating dvd iso sources list"
cat > /etc/apt/sources.list.d/delta-dvd-iso.list <<EOF
deb file:/mnt/iso precise main restricted
EOF
fi

if [ ${TARDEB} = yes ]; then
echo "Creating apt cache sources list"
cat > /etc/apt/sources.list.d/apt-cache.list <<EOF
deb file:// /var/cache/apt/archives/
EOF
fi

#else

#echo "backup existing sources list"
#cp /etc/apt/sources.list /etc/apt/sources.list.bak
#    if [ -f /etc/apt/sources.list.d/delta-dvd-iso.list ]; then
#        rm /etc/apt/sources.list.d/delta-dvd-iso.list
#    fi
#echo "Override existing sources list"
#cat > /etc/apt/sources.list <<EOF
#deb file:/mnt/iso precise main restricted
#EOF
#
#fi

sudo apt-get update
