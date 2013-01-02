#!/bin/bash

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

DEB_PATH="/var/cache/apt/archives"
STATUS_FILE="/var/log/gateway_upgrade.status"
PROGRESS_FILE="/var/log/gateway_upgrade.progress"
UPGRADE_STATUS=`cat $STATUS_FILE`
LOCK_FILE="/tmp/downloading_upgrade.lock"

if [ ! -f $LOCK_FILE -a $UPGRADE_STATUS == '5' ]
then
    touch $LOCK_FILE
    cd /tmp
    rm *.deb    ## clear old files
    apt-get download dcloud-gateway dcloudgatewayapi s3ql savebox
    if [ $? == 0]
    then
        echo "download success."
        ## change upgrade status
        echo '7' > $STATUS_FILE     ## 7 = DOWNLOAD_DONE
        mv *.deb $DEB_PATH
        dpkg-scanpackages $DEB_PATH > $DEB_PATH/Packages
        gzip -f $DEB_PATH/Packages
        apt-get update
    else
        echo '-1' > $PROGRESS_FILE     ## -1 = download failed
        ## change upgrade status
        echo '1' > $STATUS_FILE     ## 1=NO_UPGRADE_AVAILABLE
    fi
    # unlock
    rm $LOCK_FILE
else
    echo "Not DOWNLOAD_IN_PROGRESS or another process is running."
fi
