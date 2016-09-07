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
cd $here

$repo/utils/setup_dev_env.sh -v -m docker_host

# Prepare Docker build resources
sudo git clean -dXf $repo
rsync -av --delete $repo/utils/ $here/utils/
cp -f $repo/tests/functional_test/requirements.txt $here/utils/

docker build -t docker:5000/hcfs-buildbox .
docker push docker:5000/hcfs-buildbox
