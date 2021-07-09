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
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d utils ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CI_VERBOSE=false source $repo/utils/common_header.bash
cd $here

if [ "$1" = "-h" -o "$#" -eq 0 ]; then
	cat <<-EOF
	Usage:
	    ./replace_data_type.sh <branch>  # Change datatype in changed files between current branch and '<branch>'
	    ./replace_data_type.sh -a        # Change datatype in all source codes
	EOF
	exit
fi

if [ "$1" = "-a" ]; then
	files=$(find ../src ../tests/unit_test/c/unittest_modules -name '*.c' -o -name '*.h' -o -name '*.cc')
elif [ "$#" -eq 1 ]; then
	files=$(git diff $@ --name-only | sed -e 's#^#../#' | grep '.*\.c$\|.*\.h$\|.*\.cc$')
fi 


for f in $files
do
	echo $f
	sed -r -i"" \
		-e "s/\<unsigned char\>/uint8_t/g" \
		-e "s/\<unsigned long long int\>/uint64_t/g" \
		-e "s/\<unsigned long long\>/uint64_t/g" \
		-e "s/\<unsigned long int\>/uint64_t/g" \
		-e "s/\<unsigned long\>/uint64_t/g" \
		-e "s/\<unsigned short int\>/uint16_t/g" \
		-e "s/\<unsigned short\>/uint16_t/g" \
		-e "s/\<unsigned int\>/uint32_t/g" \
		-e "s/\<unsigned\>/uint32_t/g" \
		-e "s/\<signed long long int\>/int64_t/g" \
		-e "s/\<signed long long\>/int64_t/g" \
		-e "s/\<signed long int\>/int64_t/g" \
		-e "s/\<signed long\>/int64_t/g" \
		-e "s/\<signed short int\>/int16_t/g" \
		-e "s/\<signed short\>/int16_t/g" \
		-e "s/\<signed int\>/int32_t/g" \
		-e "s/\<signed\>/int32_t/g" \
		-e "s/\<long long int\>/int64_t/g" \
		-e "s/\<long long\>/int64_t/g" \
		-e "s/\<long int\>/int64_t/g" \
		-e "s/\<long\>/int64_t/g" \
		-e "s/\<short int\>/int16_t/g" \
		-e "s/\<short\>/int16_t/g" \
		-e "s/\<int\>/int32_t/g" \
		-e "/\/\*/,/\*\//s/([a-zA-Z]) \<int64_t\>/\1 long/gI" \
		-e "/\/\*/,/\*\//s/([a-zA-Z]) \<int16_t\>/\1 short/gI" \
		-e "/\/\*/,/\*\//s/const \<long\>/const int64_t/gI" \
		-e "/\/\*/,/\*\//s/const \<short\>/const short/gI" \
		"$f"
done
