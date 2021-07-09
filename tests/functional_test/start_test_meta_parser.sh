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

# keyword=test-max-pin

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
umask 000
python startMetaParserDockerTest.py
