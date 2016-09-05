#!/bin/bash

echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $repo

echo "########## Setup Test Env"
$repo/utils/setup_dev_env.sh -m docker_host
. $repo/utils/env_config.sh

echo "########## pi_tester.py"
if [ -d $repo/dist ]; then
	rm -r $repo/dist
fi
$repo/build.sh pyhcfs

echo "########## pi_tester.py"
cd $repo/tests/functional_test/TestCases/TestMetaParser/docker
python startMetaParserDockerTest.py
