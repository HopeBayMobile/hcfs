#!/bin/bash

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

DEB_PATH="/var/cache/apt/archives"
STATUS_FILE="/var/log/gateway_upgrade.status"
PROGRESS_FILE="/var/log/gateway_upgrade.progress"
PKG_VER_FILE="/var/log/downloaded_package.version"
UPGRADE_STATUS=`cat $STATUS_FILE`
LOCK_FILE="/tmp/downloading_upgrade.lock"
TMP_PATH="/tmp/debsrc"

if [ -f $LOCK_FILE ]
then
    echo "Another process is running. Exit."
    exit 1
fi

if [ $UPGRADE_STATUS == '5' ]
then
    touch $LOCK_FILE
    rm -rf $TMP_PATH    ## clear old files
    mkdir $TMP_PATH
    ## reset download progress for better GUI consistency
    echo '0' > $PROGRESS_FILE
    ## clean old deb files in cache
    apt-get clean
    ## start a thread for polling download progress
    bash /usr/local/bin/get_download_progress.sh &
    sleep 1
    ## start download package
    cd $TMP_PATH
    apt-get download dcloud-gateway dcloudgatewayapi s3ql savebox > /dev/null
    sleep 3     ## wait for get_download_progress.sh to update info
    PROGRESS=`cat $PROGRESS_FILE`
    if [ $PROGRESS -ge 100 ]
    then
        echo "download success."
        cp *.deb $DEB_PATH
        dpkg-scanpackages $DEB_PATH > $DEB_PATH/Packages
        gzip -f $DEB_PATH/Packages
        #~ apt-get update
        ## change upgrade status
        echo '7' > $STATUS_FILE     ## 7 = DOWNLOAD_DONE
    else
        #~ write an error log
        rm -rf $TMP_PATH    ## clear unfinished files
        /usr/local/TOMCAT/bin/cafeLogLiteSingle.sh API_REMOTE_UPGRADE ERROR 'Download upgrade failed. Update server disconnected during downloading upgrade package.'
        PID=$(ps aux | grep -w "get_download_progress" | head -1 | awk '{ print $2 }')
        kill -9 $PID
        echo '-1' > $PROGRESS_FILE     ## -1 = download failed
        echo 'na' > $PKG_VER_FILE     ## clean downloaded version file
    fi
    # unlock
    rm $LOCK_FILE
else
    echo "Not DOWNLOAD_IN_PROGRESS == 5."
fi
