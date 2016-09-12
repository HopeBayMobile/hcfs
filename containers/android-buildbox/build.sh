#!/bin/bash
#########################################################################
#
# Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#
# Revision History
#   2016/1/18 Jethro unified usage of workspace path
#
##########################################################################

echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
set -e -x
cd $here

sudo apt-get install pv wget

IMG=docker:5000/android-buildbox
TAG=`date +%Y%m%d`
echo $IMG
docker build --pull -t $IMG .
docker rmi $IMG:$TAG || :
docker tag $IMG $IMG:$TAG
docker push $IMG:$TAG
docker push $IMG
