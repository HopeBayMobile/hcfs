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

echo -e "\n======== ${BASH_SOURCE[0]} ========"
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && \
		while [ ! -d .git ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
source $repo/utils/common_header.bash
cd $repo

srcdir="$repo/src/HCFS $repo/src/pyhcfs"

# Install dependencies
$repo/utils/setup_dev_env.sh -m static_report,pyhcfs

hint () {
	cat <<-EOF

	==============================
	==== $1
	==============================

	EOF
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
		bear make -B -C src
		oclint-json-compilation-database . -- \
			-max-priority-1=$threshold \
			-max-priority-2=$threshold \
			-max-priority-3=$threshold \
			-report-type pmd \
			| sed "s@$repo/@@g"
	else
		bear make -s -B -C src
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
		{
			cd $repo
			LIB_DIR="" /ci-tools/pmd-bin-5.5.1/bin/run.sh cpd \
				--files src \
				--minimum-tokens 100 \
				--encoding UTF-8 --language cpp \
				--failOnViolation false \
				--format xml \
				| sed "s@$repo/@@g" >cpd-result.xml
		} && echo ${FUNCNAME[0]} Done. || echo ${FUNCNAME[0]} Fail. &
	fi
}

Report_CLOC()
{
	if [[ ${CI:-0} = 1 ]]; then
		hint ${FUNCNAME[0]}
		{
			cd $repo
			echo "Writing report to cloc-result.xml at background ..."
			cloc --by-file --xml src \
				| sed "s@$repo/@@g" > cloc-result.xml
		} && echo ${FUNCNAME[0]} Done. || echo ${FUNCNAME[0]} Fail. &
	fi
}

Report_CCM() {
	if [[ ${CI:-0} = 1 ]]; then
		hint ${FUNCNAME[0]}
		echo "Writing report to ccm-result.xml at background ..."
		{
			cd $repo
			mono /ci-tools/CCM.exe ./src /xml \
				| sed -e "s@<file>@<file>src@g" \
				-e "/^WARNING:/d" \
				-e "/^Using default runtime:/d" \
				> ccm-result.xml
		} && echo ${FUNCNAME[0]} Done. || echo ${FUNCNAME[0]} Fail. &
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
		echo "Eunning clang_scan_build at background ..."
		do_clang_scan \
			&& echo ${FUNCNAME[0]} Done. \
			|| echo ${FUNCNAME[0]} Fail. &
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
			--extensions=c,h,cpp,cc || :
	}
	if [[ ${CI:-0} = 1 ]]; then
		echo "Writing report to hb_clint.xml at background ..."
		do_hb_clint > hb_clint.xml \
			&& echo ${FUNCNAME[0]} Done. \
			|| echo ${FUNCNAME[0]} Fail. &
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
