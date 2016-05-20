#!/bin/bash
#########################################################################
#
# Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#
# Revision History
#   2016/4/25 Jethro add script to check compile dependency
#
##########################################################################
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d utils ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $repo

check_script_changes "$here/$(basename ${BASH_SOURCE[0]})"

source $here/require_compile_deps.bash
install_pkg check_pkg

commit_script_changes
commit_script_changes "$here/require_compile_deps.bash"
