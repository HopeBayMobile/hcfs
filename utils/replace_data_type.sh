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
	sed -i"" \
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
		"$f"
done
