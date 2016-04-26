#!/bin/bash
#########################################################################
#
# Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract: This script replace data types to keep cross platform compatibility.
#
# Revision History
#   2016/4/26 Jethro Added replace script
#
##########################################################################
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d utils ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CI_VERBOSE=false source $repo/utils/common_header.bash
cd $here
set -v

files=$(find ../src ../tests/unit_test/c/unittest_modules -name '*.c' -o -name '*.h' -o -name '*.cc')

for f in $files
do
	echo $f
	sed -i"" \
		-e "s/\<unsigned long long\>/uint64_t/g" \
		-e "s/\<unsigned long\>/uint64_t/g" \
		-e "s/\<unsigned int\>/uint32_t/g" \
		-e "s/\<long long\>/int64_t/g" \
		-e "s/\<long\>/int64_t/g" \
		-e "s/\<int\>/int32_t/g" \
		-e "s/\<unsigned char\>/uint8_t/g" \
		"$f"
done
