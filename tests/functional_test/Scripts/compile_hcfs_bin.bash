#!/bin/bash
echo -e "\n======== ${BASH_SOURCE[0]} ========"
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../../.. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
configfile="$here/path_config.sh"
if [ -f $configfile ]; then
	sudo chmod a+w $configfile
fi

$WORKSPACE/utils/setup_dev_env.sh -m functional_test
. $WORKSPACE/utils/env_config.sh
. $WORKSPACE/utils/trace_error.bash
set -e

echo "########## Compile binary files"
make -s -C $WORKSPACE/src/HCFS clean
make -s -C $WORKSPACE/src/CLI_utils clean
CFLAGS_ARG="`sed -rn -e '/^CFLAGS/s/ -Wall| -Wextra//g' \
	-e "/^CFLAGS/s/ *= *(.*)/='\1 -w'/p" $WORKSPACE/src/HCFS/Makefile`"
eval "make -s -C $WORKSPACE/src/HCFS $CFLAGS_ARG"

CFLAGS_ARG="`sed -rn -e '/^CFLAGS/s/ -Wall| -Wextra//g' \
	-e "/^CFLAGS/s/ *= *(.*)/='\1 -w'/p" $WORKSPACE/src/CLI_utils/Makefile`"
eval "make -s -C $WORKSPACE/src/CLI_utils $CFLAGS_ARG"

echo "########## Setup PATH for test"
if ! grep -E "(^|:)$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils(:|$)" <<< `echo $PATH`; then
	export PATH="$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils:$PATH"
	hash -r
fi

echo "export hcfs=\"`type -a hcfs | sed -s 's/.* is //'`\"" >> $configfile
echo "export HCFSvol=\"`type -a HCFSvol | sed -s 's/.* is //'`\"" >> $configfile

awk -F'=' '{seen[$1]=$0} END{for (x in seen) print seen[x]}' $configfile > awk_tmp
sudo mv -f awk_tmp $configfile
