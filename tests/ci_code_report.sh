#!/bin/bash
#########################################################################
#
# Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#
# Revision History
#   2016/1/18 Jethro unified usage of workspace path
#
##########################################################################

echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $repo

sudo git clean -dxf
pwd
whoami
ls -la

srcdir="$repo/src/HCFS $repo/src/pyhcfs"
testdir=$repo/tests

# Install dependencies
$repo/utils/setup_dev_env.sh -vm static_report

hint () {
	echo ==============================
	echo ==============================
	echo ==============================
	echo $1
}

Report_Oclint() {
	hint ${FUNCNAME[0]}
	PATH=$PATH:/ci-tools/oclint-0.8.1/bin:/usr/local/bin
	for dir in $srcdir; do
		bear make -j4 -C $dir
		oclint-json-compilation-database . -- -report-type pmd -o $dir/oclint-report.xml || :
		sed -i "s@$repo/@@g" $dir/oclint-report.xml
	done
}

Report_CPD() {
	hint ${FUNCNAME[0]}
	for dir in $srcdir; do
		LIB_DIR="" /ci-tools/pmd-bin-5.2.2/bin/run.sh cpd \
			--minimum-tokens 100 --encoding UTF-8 --files $dir \
			--format xml --language cpp \
			> $dir/cpd-result.xml || return_code=$?
		if [[ "$return_code" != "" && "$return_code" != 4 ]]; then
			echo pmd returns unknown error code $return_code
			false
		fi
		sed -i "s@$repo/@@g" $repo/cpd-result.xml
	done
}

Report_CLOC()
{
	hint ${FUNCNAME[0]}
	cloc --by-file --xml --out=$repo/cloc-result.xml $repo/src
	sed -i "s@$repo/@@g" $repo/cloc-result.xml
}

Report_CCM() {
	hint ${FUNCNAME[0]}
	mono /ci-tools/CCM.exe $repo/src /xml > $repo/ccm-result.xml
	sed -i -e "s@<file>/@<file>src/HCFS/@g" \
		-e "/^WARNING:/d" \
		-e "/^Using default runtime:/d" \
		$repo/ccm-result.xml
}

Style_Checking_With_hb_clint() {
	hint ${FUNCNAME[0]}
	find $srcdir -name "*.c" -or -name "*.cpp" |\
		parallel --jobs 4 $testdir/code_checking/Style_Checking_With_hb_clint.py \
		{} > $repo/Style_Checking_With_hb_clint.xml 2>&1 || :
	sed -i "s@$repo/@@g" $repo/Style_Checking_With_hb_clint.xml
}

Style_Checking_With_checkpatch() {
	hint ${FUNCNAME[0]}
	find $srcdir -name "*.c" -or -name "*.cpp" |\
		parallel --jobs 4 $testdir/code_checking/Style_Checking_With_checkpatch.pl \
		--terse --no-tree --no-signoff -f {} || :
}

Report_clang_scan_build() {
	hint ${FUNCNAME[0]}
	for dir in $srcdir; do
		rm -rf $repo/clangScanBuildReports
		cd $dir
		make clean
		scan-build-3.5 -o $repo/clangScanBuildReports make -j4
	done
}

Report_Oclint
Report_clang_scan_build
Report_CPD
Report_CLOC
Report_CCM
Style_Checking_With_hb_clint
Style_Checking_With_checkpatch
