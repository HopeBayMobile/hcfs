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
cd $repo

# Remove files thail will fail tests
sudo rm -rf /tmp/tmpdir /tmp/testHCFS /tmp/this_met /tmp/test_fuse /tmp/FSmgr_upload /tmp/markdelete /tmp/root_meta_path /tmp/test_dir_meta /tmp/testmeta /tmp/test /tmp/testmount /tmp/tmp_meta_dir /tmp/FSmgr_upload /tmp/test1 /tmp/mock_dir_meta /tmp/mock_path /tmp/mock_meta_used_in_dir_add_entry /tmp/mock_meta_path /tmp/dir_meta_path /tmp/directory_creation /tmp/tmp_dir_meta /tmp/test_system_file /tmp/xattr_mock_meta /tmp/dir_remove_entry_meta_file /tmp/mock_symlink_meta /tmp/mock_meta /tmp/here_is_obj /tmp/this_meta /tmp/to_delete_meta /tmp/readdir_meta /tmp/tmpout /tmp/testlog || :

if [ -f /.dockerinit ] && id -u jenkins >/dev/null 2>&1; then
	echo "jenkins ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/50_jenkins_sh
	sudo -E -u jenkins run-parts --exit-on-error --verbose $here/scrips
else
	run-parts --exit-on-error --verbose $here/scrips
fi
