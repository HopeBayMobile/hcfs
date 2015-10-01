#!/bin/bash
echo ======== ${BASH_SOURCE[0]} ========
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../.. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

. $WORKSPACE/utils/trace_error.bash
set -e

echo "########## Setup Test Env"
$WORKSPACE/utils/setup_dev_env.sh -m functional_test
. $WORKSPACE/utils/env_config.sh

$here/Scripts/compile_hcfs_bin.bash
# Setup for $hcfs and $HCFSvol
. $here/Scripts/path_config.sh


$here/Scripts/start_swift.bash
$here/Scripts/test_mount.bash

cd "$here"
#python pi_tester.py -d debug -c HCFS_0
