#!/bin/bash

echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

sudo chmod 777 -R $here/TestCases/Utils/report
sudo chmod 777 -R $repo/tests/functional_test/Reports
