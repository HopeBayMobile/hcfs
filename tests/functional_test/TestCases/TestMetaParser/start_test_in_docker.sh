#!/bin/bash

repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"

python3 -m easy_install $repo/dist/*

cd $repo/tests/functional_test

pip install -r requirements.txt

umask 000

python pi_tester.py -s TestSuites/TestMetaParser.csv
