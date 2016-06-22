#!/bin/bash

repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
dockerfile=$repo/tests/functional_test/TestCases/TestMetaParser/Dockerfile
image="test_img"
ver="1.0"

if [ ! -e /var/run/docker.sock ]; then
	echo "Missing installed docker or docker daemon is not running."
	exit -1
fi

if [[ $(docker images | grep $image | grep $ver) ]]; then
	echo "Build docker image for test environment."
	cd $repo
	docker build -f $dockerfile -t $image\:$ver .
fi

cd $repo/tests/functional_test/TestCases/TestMetaParser
python DockerTest.py
