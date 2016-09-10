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
	echo ""
	echo "=============================="
	echo "==== $1"
	echo "=============================="
	echo ""
}

set_PARALLEL_JOBS()
{
	if hash nproc; then
		_nr_cpu=`nproc`
	else
		_nr_cpu=`cat /proc/cpuinfo | grep processor | wc -l`
	fi
	export PARALLEL_JOBS="-l ${_nr_cpu}.5"
}

Report_Oclint() {
	hint ${FUNCNAME[0]}
	threshold=9999
	cd $repo
	PATH=/ci-tools/oclint-0.10.3/bin:$PATH
	if [[ ${CI:-0} = 1 ]]; then
		echo "Writing report to ccm-result.xml at background ..."
		{
			bear make -s -B $PARALLEL_JOBS -C src &>/dev/null
			oclint-json-compilation-database . -- \
				-max-priority-1=$threshold \
				-max-priority-2=$threshold \
				-max-priority-3=$threshold \
				-report-type pmd \
				2>/dev/null \
				| sed "s@$repo/@@g" > oclint-report.xml
		}&
	else
		bear make -s -B $PARALLEL_JOBS -C src
		oclint-json-compilation-database . -- \
			-max-priority-1=$threshold \
			-max-priority-2=$threshold \
			-max-priority-3=$threshold \
			2>/dev/null
	fi
}

Report_CPD() {
	if [[ ${CI:-0} = 1 ]]; then
		hint ${FUNCNAME[0]}
		echo "Writing report to cpd-result.xml at background ..."
		cd $repo
		LIB_DIR="" /ci-tools/pmd-bin-5.5.1/bin/run.sh cpd \
			--files src \
			--minimum-tokens 100 \
			--encoding UTF-8 --language cpp \
			--failOnViolation false \
			--format xml 2>/dev/null \
			| sed "s@$repo/@@g" >cpd-result.xml &
	fi
}

Report_CLOC()
{
	if [[ ${CI:-0} = 1 ]]; then
		hint ${FUNCNAME[0]}
		cd $repo
		echo "Writing report to cloc-result.xml at background ..."
		cloc --by-file --xml src \
		| sed "s@$repo/@@g" > cloc-result.xml &
	fi
}

Report_CCM() {
	if [[ ${CI:-0} = 1 ]]; then
		hint ${FUNCNAME[0]}
		echo "Writing report to ccm-result.xml at background ..."
		cd $repo
		mono /ci-tools/CCM.exe ./src /xml \
			| sed -e "s@<file>@<file>src@g" \
			-e "/^WARNING:/d" \
			-e "/^Using default runtime:/d" \
			> ccm-result.xml &
	fi
}

Report_clang_scan_build() {
	hint ${FUNCNAME[0]}
	do_clang_scan()
	{
		rm -rf $repo/clangScanBuildReports
		scan-build-4.0 -o $repo/clangScanBuildReports \
			make -B -l 2.5 -C $repo/src
		find $repo/clangScanBuildReports -type f \
			| xargs sed -i "s@$repo/@@g"
	}
	if [[ ${CI:-0} = 1 ]]; then
		echo "Writing report to hb_clint.xml at background ..."
		do_clang_scan &>/dev/null &
	else
		do_clang_scan
	fi
}

Style_Checking_With_hb_clint() {
	hint ${FUNCNAME[0]}
	do_hb_clint()
	{
		cd $repo
		find src -iregex '.*\.\(c\|h\|cpp\|cc\)' \
			| xargs tests/code_checking/hb_clint.py \
			--counting=detailed \
			--extensions=c,h,cpp,cc
	}
	if [[ ${CI:-0} = 1 ]]; then
		echo "Writing report to hb_clint.xml at background ..."
		do_hb_clint > hb_clint.xml 2>&1 &
	else
		do_hb_clint || :
	fi

}

Style_Checking_With_checkpatch() {
	hint ${FUNCNAME[0]}
	find $srcdir -iregex '.*\.\(c\|h\|cpp\|cc\)' |\
		xargs $repo/tests/code_checking/checkpatch.pl \
		--terse --no-tree --no-signoff --no-color -f || :
}

set_PARALLEL_JOBS

Report_Oclint
Report_CPD
Report_CLOC
Report_CCM
Report_clang_scan_build
Style_Checking_With_hb_clint

# Report that jenkins cannot parse
Style_Checking_With_checkpatch
wait
