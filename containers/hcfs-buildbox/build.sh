#!/bin/bash
##
## Copyright (c) 2021 HopeBayTech.
##
## This file is part of Tera.
## See https://github.com/HopeBayMobile for further info.
##
## Licensed under the Apache License, Version 2.0 (the "License");
## you may not use this file except in compliance with the License.
## You may obtain a copy of the License at
##
##     http://www.apache.org/licenses/LICENSE-2.0
##
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##

echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $here

$repo/utils/setup_dev_env.sh -m docker_host

set -x
if [ -f internal.tgz ]; then
	rm -rf internal_last
	mkdir -p internal_last && pushd internal_last
	tar zxvf ../internal.tgz
	popd
fi

rsync -ra -t --delete $repo/utils/ internal/utils/
cp -f $repo/tests/functional_test/requirements.txt internal/utils/

if ! diff -aru internal/ internal_last/; then
	echo Update internal.tgz
	rm -f internal.tgz
fi

if [ ! -f internal.tgz ]; then
	( cd internal/; tar zcvf ../internal.tgz * )
fi

# id_rsa
if [ ! -f id_rsa ]; then
	unzip id_rsa.zip
fi

docker build -t docker:5000/hcfs-buildbox .
docker push docker:5000/hcfs-buildbox
