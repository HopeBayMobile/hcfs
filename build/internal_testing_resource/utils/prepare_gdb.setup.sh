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
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$here"/..

# Append gdb.setup
cat >> gdb.setup <<EOF
set sysroot .
target remote :5678
cont
EOF

# Copy dependent source
set `cat gdb.setup | grep ^directory`
shift
for i in $@
do
	mkdir -p ./$i
	rsync -ra $i/ ./$i/
done
sed -i -e "s# /# #g" gdb.setup
cat gdb.setup
