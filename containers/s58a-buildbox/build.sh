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
set -e -x
cd $here

SRC_VERSION=v5.0419
SRC_IMG=docker:5000/s58a-buildbox:source-only
echo $SRC_IMG
docker build -t $SRC_IMG .
docker tag $SRC_IMG $SRC_IMG-$SRC_VERSION
{ exec > /dev/null && docker push $SRC_IMG && docker push $SRC_IMG-$SRC_VERSION; }&


for TYPE in user userdebug
do
	PREBUILT_IMG=docker:5000/s58a-buildbox:${TYPE}-prebuilt
	docker build -f Dockerfile.${TYPE} -t $PREBUILT_IMG .
	docker tag $PREBUILT_IMG $PREBUILT_IMG-$SRC_VERSION
	{ exec > /dev/null && docker push $PREBUILT_IMG && docker push $PREBUILT_IMG-$SRC_VERSION; }&
done

docker push docker:5000/s58a-buildbox:source-only
docker push docker:5000/s58a-buildbox:source-only-$SRC_VERSION
docker push docker:5000/s58a-buildbox:user-prebuilt
docker push docker:5000/s58a-buildbox:user-prebuilt-$SRC_VERSION
docker push docker:5000/s58a-buildbox:userdebug-prebuilt
docker push docker:5000/s58a-buildbox:userdebug-prebuilt-$SRC_VERSION
