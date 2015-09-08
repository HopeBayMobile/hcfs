#!/bin/bash
echo ======== ${BASH_SOURCE[0]} ========
set -x
OLDUID=${SUDO_UID:-$UID}
OLDGID=${SUDO_GID:-$GROUPS}
NEWUID=$(stat -c '%u' /home/jenkins/workspace/HCFS)
NEWGID=$(stat -c '%g' /home/jenkins/workspace/HCFS)
if [[ $OLDUID != $NEWUID || $OLDGID != $NEWGID ]]; then
    echo FIX file permission in docker
    echo "[Before]"
    ls -l /home/jenkins/workspace/HCFS
    sudo usermod -u $NEWUID jenkins
    sudo groupmod -g $NEWGID jenkins
    sudo find $HOME -user $OLDUID -exec chown -h $NEWUID {} \;
    sudo find $HOME -group $OLDGID -exec chgrp -h $NEWGID {} \;
    echo "[After]"
    ls -l /home/jenkins/workspace/HCFS
fi
