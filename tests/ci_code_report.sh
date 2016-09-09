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
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && \
		while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $repo

srcdir="$repo/src/HCFS $repo/src/pyhcfs"

# Install dependencies
$repo/utils/setup_dev_env.sh -m static_report

hint () {
	echo ==============================
	echo ==============================
	echo ==== $1
	echo ==============================
	echo ==============================
}

Report_Oclint() {
	hint ${FUNCNAME[0]}
	cd $repo
	PATH=/ci-tools/oclint-0.8.1/bin:$PATH
	bear make -s -B -l 2.5 -C src
	maxPriority=9999
	oclint-json-compilation-database . -- \
		-max-priority-1=$maxPriority \
		-max-priority-2=$maxPriority \
		-max-priority-3=$maxPriority &
	oclint-json-compilation-database . -- \
		-max-priority-1=$maxPriority \
		-max-priority-2=$maxPriority \
		-max-priority-3=$maxPriority \
		-report-type pmd \
		-o oclint-report.xml 2>/dev/null &
	wait
	sed -i "s@$repo/@@g" oclint-report.xml
}

Report_CPD() {
	hint ${FUNCNAME[0]}
	cd $repo
	LIB_DIR="" /ci-tools/pmd-bin-5.5.1/bin/run.sh cpd \
		--files src \
		--minimum-tokens 100 \
		--encoding UTF-8 --language cpp \
		--failOnViolation false \
		--format xml \
		| sed "s@$repo/@@g" > cpd-result.xml
}

Report_CLOC()
{
	hint ${FUNCNAME[0]}
	cloc --by-file --xml --out=$repo/cloc-result.xml $repo/src
	sed -i "s@$repo/@@g" $repo/cloc-result.xml
}

Report_CCM() {
	hint ${FUNCNAME[0]}
	cd $repo
	mono /ci-tools/CCM.exe ./src /xml > ccm-result.xml
	sed -i -e "s@<file>@<file>src@g" \
		-e "/^WARNING:/d" \
		-e "/^Using default runtime:/d" \
		$repo/ccm-result.xml
}

Report_clang_scan_build() {
	hint ${FUNCNAME[0]}
	rm -rf $repo/clangScanBuildReports
	scan-build-4.0 -o $repo/clangScanBuildReports \
		make -B -l 2.5 -C $repo/src
	find $repo/clangScanBuildReports -type f | xargs sed -i "s@$repo/@@g"
}

Style_Checking_With_hb_clint() {
	hint ${FUNCNAME[0]}
	cd $repo
	find src -iregex '.*\.\(c\|h\|cpp\|cc\)' |\
		xargs tests/code_checking/hb_clint.py \
		--counting=detailed --extensions=c,h,cpp,cc\
		| tee hb_clint.xml 2>&1 || :
}

Style_Checking_With_checkpatch() {
	hint ${FUNCNAME[0]}
	find $srcdir -iregex '.*\.\(c\|h\|cpp\|cc\)' |\
		xargs $repo/tests/code_checking/checkpatch.pl \
		--terse --no-tree --no-signoff -f || :
}

Report_Oclint
Report_CPD
Report_CLOC
Report_CCM
Report_clang_scan_build
Style_Checking_With_hb_clint
Style_Checking_With_checkpatch
