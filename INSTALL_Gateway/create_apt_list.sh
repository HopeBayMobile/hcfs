# cat create_apt_list.sh
#!/bin/bash

# mount precise dvd iso
INTERNET=no
if [ ! -d /mnt/iso ]; then
    sudo mkdir -p /mnt/iso
else
    sudo umount /mnt/iso
fi
sudo mount -t iso9660 -o ro,loop=/dev/loop7 /iso/ubuntu-12.04-dvd-amd64.iso /mnt/iso

if [ ${INTERNET} = "yes" ]; then

echo "Creating sources list"
cat > /etc/apt/sources.list.d/delta-dvd-iso.list <<EOF
deb file:/mnt/iso precise main restricted
EOF

else

echo "backup existing sources list"
cp /etc/apt/sources.list /etc/apt/sources.list.bak
    if [ -f /etc/apt/sources.list.d/delta-dvd-iso.list ]; then
        rm /etc/apt/sources.list.d/delta-dvd-iso.list
    fi
echo "Override existing sources list"
cat > /etc/apt/sources.list <<EOF
deb file:/mnt/iso precise main restricted
EOF

fi

sudo apt-get update
