#!/bin/bash
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd "$here"/..
set `cat gdb.setup | grep ^directory`
shift
for i in $@
do
	mkdir -p ./$i
	rsync -ra $i/ ./$i/
done
sed -i -e "s# /# #g" gdb.setup
cat gdb.setup
