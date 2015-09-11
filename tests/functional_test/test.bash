#!/bin/bash
echo ======== ${BASH_SOURCE[0]} ========
set -x -e

WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../.. && pwd )"

$WORKSPACE/utils/setup_dev_env.sh -m functional_test

make -C $WORKSPACE/src/HCFS
make -C $WORKSPACE/src/CLI_utils
PATH="$WORKSPACE/src/HCFS:$WORKSPACE/src/CLI_utils:$PATH"

python pi_tester.py -s TestSuites/HCFS.csv
