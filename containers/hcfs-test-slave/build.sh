#!/bin/bash

repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../.. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

set -x -e
$repo/utils/setup_dev_env.sh -v -m docker_host
sudo git clean -dXf $repo

rsync -avz --delete ../../utils/ $here/utils/
cp -f ../functional_test/requirements.txt $here/utils/
docker build -t docker:5000/docker_hcfs_test_slave .
docker push docker:5000/docker_hcfs_test_slave
