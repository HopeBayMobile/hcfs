#!/bin/bash
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
