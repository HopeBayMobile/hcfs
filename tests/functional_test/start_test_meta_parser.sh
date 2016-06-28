#!/bin/bash

repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"

$repo/utils/setup_dev_env.sh -m docker_host

$repo/build.sh pyhcfs

#dockerfile=$repo/tests/functional_test/TestCases/TestMetaParser/Dockerfile
#image="test_img"
#ver="1.0"
#if [[ ! $(docker images | grep $image | grep $ver) ]]; then
#	echo "Build docker image for test environment."
#	cd $repo/..
#	docker build -f $dockerfile -t $image\:$ver .
#fi

cd $repo/tests/functional_test/TestCases/TestMetaParser
python prepare.py
python DockerTest.py
