#!/bin/bash

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

PROGRESS_FILE="/var/log/gateway_upgrade.progress"
STATUS_FILE="/var/log/gateway_upgrade.status"
TMP_PATH="/tmp/debsrc"

## get the size of deb files
    apt-get download --print-uris dcloud-gateway dcloudgatewayapi s3ql savebox > /tmp/deb.info
    IFS=" "
    TOTAL_SIZE=0
    while read URL NAME SIZE SHA
    do
        let TOTAL_SIZE=$TOTAL_SIZE+$SIZE 
    done < /tmp/deb.info
    #~ echo $TOTAL_SIZE

## update download progress
    UPGRADE_STATUS=`cat $STATUS_FILE`
    DOWNLOADED=0
    while [ $UPGRADE_STATUS == '5' ]
    do
        DOWNLOADED=`du -s $TMP_PATH |  cut -d '/' -f 1`
        echo $DOWNLOADED
        PROGRESS=$(echo "scale=0; 100.0*$DOWNLOADED/$TOTAL_SIZE" | bc -l)
        echo $PROGRESS
        
        UPGRADE_STATUS=`cat $STATUS_FILE`
        sleep 1
    done
