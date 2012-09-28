#!/bin/bash
echo "==========================================================="
echo "=== This script will create a package of all"
echo "    debian files which have been installed on a machine ==="
echo "==========================================================="

# Make sure only root can run this script
    if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
       exit 1
    fi

# declare constants
    LIST_FILE="pkg_list.txt"
    DEB_DIR="debsrc"

# create a list of installed packages
    mkdir $DEB_DIR
    cd $DEB_DIR
    dpkg --get-selections > $LIST_FILE
    sleep 1

# parse the list
    while read line; do 
        PKG=${line%'install'} 
        echo $PKG
        apt-get download $PKG
    done < $LIST_FILE

# clean up files
    rm $LIST_FILE
