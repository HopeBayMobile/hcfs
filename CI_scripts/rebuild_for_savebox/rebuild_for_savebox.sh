#!/bin/bash

## Usage:
## ./rebuild_for_savebox.sh <original_gw_package> <savebox_deb_file>

# make sure input arguments are correct
    if [ $# -ne 2 ]
    then
        echo "Need to input 2 arguments. Such as:"
        echo "./rebuild_for_savebox.sh </path/original_gw_package> </path/savebox_deb_file>"
        exit 1
    fi

# define parameters
    PKG_SOURCE=$1       # original gateway package
    SAVBOX_SOURCE=$2    # savebox's new DEB file
    NEW_PKG="new_gateway_install_pkg.tar"
    TMP_DIR="/tmp/rebuild_pkg"
    APTCACHEDIR="$TMP_DIR/debsrc"
    INITPATH=$(pwd)
    
# Untar original gateway package
    rm -rf $TMP_DIR
    mkdir $TMP_DIR
    tar -xf $PKG_SOURCE -C $TMP_DIR
    
# Untar debian source file
    cd $TMP_DIR
    mkdir debsrc
    DEBFILE=`ls debsrc_StorageAppliance*.tgz`
    tar -xzf $DEBFILE -C ./debsrc
    rm $DEBFILE     ## clean old DEB file

# replace savebox file
    rm $APTCACHEDIR/savebox*.deb
    cp $SAVBOX_SOURCE $APTCACHEDIR 
    dpkg-scanpackages ${APTCACHEDIR} > ${APTCACHEDIR}/Packages
    gzip -f ${APTCACHEDIR}/Packages     ## rebuild index for APT
    
# Back up all DEB files in /var/cache/apt/archives/ with the source code
# tar the DEB packages into a file.
    echo "packing DEB files"
    cd $APTCACHEDIR
    tar -czf $DEBFILE *
    mv $DEBFILE $TMP_DIR
    cd $TMP_DIR
    rm -rf $APTCACHEDIR   # clean debsrc folder

## pack a new tar file
    cd $TMP_DIR
    echo "creating a new gateway installation package"
    tar -cf $NEW_PKG *
    echo "DONE"
    echo "You can find new package at /tmp/rebuild_pkg/new_gateway_install_pkg.tgz"

