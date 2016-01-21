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

exec 1> >(while read line; do echo -e "        $line"; done;)
exec 2> >(while read line; do echo -e "        $line" >&2; done;)

echo "########## Setup Test Env"
$repo/utils/setup_dev_env.sh -m functional_test
. $repo/utils/env_config.sh

# Main cource code
configfile="$here/path_config.sh"
if [ -f $configfile ]; then
	sudo chown $USER $configfile
fi

echo "########## Compile binary files"
make -s -C $repo/src/HCFS clean
make -s -C $repo/src/CLI_utils clean
CFLAGS_ARG="`sed -rn -e '/^CFLAGS/s/ -Wall| -Wextra//g' \
	-e "/^CFLAGS/s/ *= *(.*)/='\1 -w'/p" $repo/src/HCFS/Makefile`"
eval "make -s -C $repo/src/HCFS $CFLAGS_ARG"

CFLAGS_ARG="`sed -rn -e '/^CFLAGS/s/ -Wall| -Wextra//g' \
	-e "/^CFLAGS/s/ *= *(.*)/='\1 -w'/p" $repo/src/CLI_utils/Makefile`"
eval "make -s -C $repo/src/CLI_utils $CFLAGS_ARG"

echo "########## Setup PATH for test"
if ! grep -E "(^|:)$repo/src/HCFS:$repo/src/CLI_utils(:|$)" <<< `echo $PATH`; then
	echo "export PATH=\"$repo/src/HCFS:$repo/src/CLI_utils:$PATH\"" >> $configfile
	export PATH="$repo/src/HCFS:$repo/src/CLI_utils:$PATH"
fi

echo "export hcfs=\"`type -a hcfs | head -1 | sed -s 's/.* is //'`\"" >> $configfile
echo "export HCFSvol=\"`type -a HCFSvol | head -1 | sed -s 's/.* is //'`\"" >> $configfile

awk -F'=' '{seen[$1]=$0} END{for (x in seen) print seen[x]}' $configfile > awk_tmp
sudo mv -f awk_tmp $configfile
