#!/bin/bash
echo ==== ci.sh =====================================================================
set -ex
export repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd ../.. && pwd )"
WORKSPACE=${WORKSPACE:-$repo}

cd $WORKSPACE
pwd
whoami
ls -la

srcdir=$WORKSPACE/src
testdir=$WORKSPACE/tests

# Install dependencies
$WORKSPACE/utils/setup_dev_env.sh

# Using OCLint with Bear
bear make -C $srcdir/HCFS
/oclint-0.8.1/bin/oclint-json-compilation-database . -- -report-type pmd -o oclint-report.xml -max-priority-1=0 -max-priority-2=50 -max-priority-3=100 || true
sed -i 's/file name="/&src\/HCFS\//g' oclint-report.xml

# PMD's Copy/Paste Detector (CPD)
sudo /pmd-bin-5.2.2/bin/run.sh cpd --minimum-tokens 100 --encoding UTF-8 --files $srcdir/ --format xml --language cpp > $WORKSPACE/cpd-result.xml || true

# Publish SLOCCount analysis results
sudo cloc --by-file --xml --out=$WORKSPACE/cloc-result.xml $srcdir/HCFS/

# Publish CCM analysis results (Cyclomatic Complexity)
sudo mono /CCM.exe . /xml > $WORKSPACE/ccm-result.xml

# Run unit test
cd $testdir/unit_test
./run_unittests

# Unit test coverage report
gcovr -x -r $srcdir/HCFS . > $WORKSPACE/coverage.xml

# Cehck code style
$testdir/code_checking/hb_clint.py --output=vs7 `find $srcdir -name '*.[ch]'` 2>&1 | tee $WORKSPACE/hb_clint.xml
$testdir/code_checking/checkpatch.pl --terse --no-tree --no-signoff -f `find $srcdir -name '*.[ch]'` || true
