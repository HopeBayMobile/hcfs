#!/bin/bash
set -x -e
cp -f ../../utils/setup_dev_env.sh .
cp -f ../../tests/functional_test/requirements.txt .
docker build -t docker:5000/docker_hcfs_test_slave .
docker push docker:5000/docker_hcfs_test_slave
