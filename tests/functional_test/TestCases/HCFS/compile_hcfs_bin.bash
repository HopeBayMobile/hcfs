#!/bin/bash
exec 1> >(while read line; do echo -e "        $line"; done;)
exec 2> >(while read line; do echo -e "        $line" >&2; done;)
set -e
echo -e "\n======== ${BASH_SOURCE[0]} ========"
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../../../.. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo "########## Setup Test Env"
. $WORKSPACE/utils/trace_error.bash
$WORKSPACE/utils/setup_dev_env.sh -m functional_test
. $WORKSPACE/utils/env_config.sh

# Main cource code
configfile="$here/path_config.sh"
if [ -f $configfile ]; then
	sudo chown $USER $configfile
fi

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
	echo "export PATH=\"$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils:$PATH\"" >> $configfile
	export PATH="$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils:$PATH"
fi

echo "export hcfs=\"`type -a hcfs | head -1 | sed -s 's/.* is //'`\"" >> $configfile
echo "export HCFSvol=\"`type -a HCFSvol | head -1 | sed -s 's/.* is //'`\"" >> $configfile

awk -F'=' '{seen[$1]=$0} END{for (x in seen) print seen[x]}' $configfile > awk_tmp
sudo mv -f awk_tmp $configfile
