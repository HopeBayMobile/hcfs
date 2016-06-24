#!/bin/bash

repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"

easy_install $repo/dist/pyhcfs-0.1.dev0-py2.7-linux-x86_64.egg

cd $repo/tests/functional_test
python pi_tester.py -d debug -s TestSuites/TestMetaParser.csv
