#!/bin/bash

repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"

$repo/utils/setup_dev_env.sh -m docker_host

rm -r $repo/dist

$repo/build.sh pyhcfs

cd $repo/tests/functional_test/TestCases/TestMetaParser/docker

# python prepare.py

python startMetaParserDockerTest.py
