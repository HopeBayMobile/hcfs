#!/bin/bash
# vim:set tabstop=4 shiftwidth=4 softtabstop=0 noexpandtab:
#########################################################################
#
# Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#
# Revision History
#   2016/1/17 Jethro hide debug infomation
#
##########################################################################
set +x
repo="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && while [ ! -d utils ] ; do cd ..; done; pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
CI_VERBOSE=false source $repo/utils/common_header.bash
cd $repo

configfile="$repo/utils/env_config.sh"
touch "$configfile"

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

# Initialize our own variables:
output_file=""
export verbose=${verbose:=0}

while getopts ":vm:" opt; do
	case $opt in
	m)
		export setup_dev_env_mode=$OPTARG
		;;
	v)
		export verbose=1
		;;
	\?)
		echo "Invalid option: -$OPTARG" >&2
		exit 1
		;;
	:)
		echo "Option -$OPTARG requires an argument." >&2
		exit 1
		;;
	esac
done

check_script_changes "$here/$(basename ${BASH_SOURCE[0]})"

if [ $verbose -eq 0 ]; then
	set +x;
else
	echo -e "\n======== ${BASH_SOURCE[0]} mode $setup_dev_env_mode ========"
	set -x;
fi

static_report() {
	export post_pkg_install+=" post_static_report"
	packages+=" cmake git build-essential" # Required by oclint / bear
	packages+=" wget unzip"                # Required by PMD for CPD(duplicate code)
	if lsb_release -r | grep -q 16.04; then
		packages+=" openjdk-8-jdk "    # Required by PMD for CPD(duplicate code)
	else
		packages+=" openjdk-7-jdk "    # Required by PMD for CPD(duplicate code)
	fi
	packages+=" cloc"                      # Install cloc for check code of line
	packages+=" mono-complete"             # Required mono and CCM for complexity
	packages+=" colormake"                 # colorful logs
	packages+=" parallel"                  # parallel check source code style
	local CLANG_V=4.0
	packages+=" clang-${CLANG_V} clang-format-${CLANG_V}"

	# Add repo for Clang Scan Build
	if ! dpkg -s clang-${CLANG_V} >/dev/null 2>&1; then
		source /etc/lsb-release
		DIST=$DISTRIB_CODENAME
		cat <<-EOF |
		deb http://apt.llvm.org/${DIST}/ llvm-toolchain-${DIST} main
		deb-src http://apt.llvm.org/${DIST}/ llvm-toolchain-${DIST} main
		EOF
		sudo tee /etc/apt/sources.list.d/llvm.list
		curl http://apt.llvm.org/llvm-snapshot.gpg.key |\
			sudo apt-key add -
	fi
}
post_static_report() {

	# Patch clang-format
	clang_format_py=/usr/share/vim/addons/syntax/clang-format-4.0.py
	if [ -f $clang_format_py ] && \
		! grep -q sys.setdefaultencoding $clang_format_py;
	then
		(cd / && sudo patch -N -p0 < $here/clang-format.patch)
	fi

	# Setup ci-tools
	sudo mkdir -p /ci-tools
	sudo chmod 777 /ci-tools
	pushd /ci-tools

	#### Oclint
	if ! hash bear; then
		git clone --depth 1 https://github.com/rizsotto/Bear.git || :
		pushd Bear
		rm -f CMakeCache.txt
		cmake .
		make all
		sudo make install
		popd
	fi
	if [ ! -f oclint-0.10.3/bin/oclint ]; then
		URL=ftp://172.16.10.200/ubuntu/CloudDataSolution/HCFS_android/resources/oclint-0.10.3-x86_64-linux-3.13.0-74-generic.tar.gz
		wget $URL && tar -zxf ${URL##*/} && rm -f ${URL##*/}
	fi

	#### Install PMD for CPD(duplicate code)
	if [ ! -d pmd-bin-5.5.1 ]; then
		URL=ftp://172.16.10.200/ubuntu/CloudDataSolution/HCFS_android/resources/pmd-bin-5.5.1.zip
		wget $URL && unzip ${URL##*/} && rm -f ${URL##*/}
	fi

	#### Install mono and CCM for complexity
	if [ ! -f CCM.exe ]; then
		URL=ftp://172.16.10.200/ubuntu/CloudDataSolution/HCFS_android/resources/ccm.1.1.11.zip
		wget $URL && unzip ${URL##*/} && rm -f ${URL##*/}
	fi

	popd
}

unit_test() {
	packages+=" gcovr"
	packages+=" valgrind"
}

functional_test() {
	$here/nopasswd_sudoer.bash
	packages+=" python-pip python-dev python-swiftclient"
	export post_pkg_install+=" post_install_pip_packages"

	packages+=" openssl units pv" # To generate large file

	echo "########## Setup Fuse config"
	if sudo grep "#user_allow_other" /etc/fuse.conf; then
		sudo sed -ir -e "s/#user_allow_other/user_allow_other/" /etc/fuse.conf
	fi
	if ! grep -q "^fuse:" /etc/group; then
		sudo addgroup fuse
	fi
	U=${SUDO_USER:-$USER}
	if [[ -n "$U" && "$U" != root && `groups $U` != *fuse* ]]; then
		sudo adduser $U fuse
	fi
}
post_install_pip_packages() {
	file=`\ls $here/requirements.txt \
		$repo/tests/functional_test/requirements.txt \
		2>/dev/null ||:`

	sudo -H pip install -q -r $file
}

install_ccache() {
	local VER=`ccache -V 2>&1 | sed -r -n -e "s/.* ([0-9.]+)$/\1/p"`
	if ! expr $VER \>= 3.2 2>/dev/null; then
		force_install="$force_install ccache"
	fi
	local APT_VER=`apt-cache policy ccache 2>&1 | \
		sed -r -n -e "/Candidate/s/.* ([0-9.]+).*/\1/p"`
	if [ "$DISTRIB_RELEASE" = "14.04" ] && \
		! expr $APT_VER \>= 3.2 2>/dev/null; then
		source /etc/lsb-release
		echo "deb http://ppa.launchpad.net/comet-jc/ppa/ubuntu $DISTRIB_CODENAME main" \
			| sudo tee /etc/apt/sources.list.d/comet-jc-ppa-trusty.list
		sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 32EF5841642ADD17
	fi
}

docker_host() {
	packages+=" wget"
	export post_pkg_install+=" post_install_docker"
}
post_install_docker() {
	# skip is docker version is qualified
	set -x
	if hash docker && \
		expr `docker version | sed -n -r -e "/Client/{N;s/[^0-9]*(.*)/\1/p}"` \>= 1.11.2 >&/dev/null; then
		NEED_INSTALL_DOCKER=false
	fi

	if ${NEED_INSTALL_DOCKER:-true}; then
		sudo apt-key adv --recv-key --keyserver keyserver.ubuntu.com \
			58118E89F3A912897C070ADBF76221572C52609D
		wget -qO- https://get.docker.com/ > /tmp/install_docker
		sed -i 's#apt-get install#& -o Dpkg::Options::=--force-confdef#' \
			/tmp/install_docker
		sudo sh /tmp/install_docker
	fi

	# Setup Docker config
	if ! sudo grep -q "docker:5000" /etc/docker/daemon.json; then
		echo "Updating /etc/docker/daemon.json"
		cat <<-EOF |
		{ "insecure-registries":[
			"docker.hopebaytech.com:5000",
			"docker:5000" ] }
		EOF
		sudo tee /etc/docker/daemon.json
		sudo service docker restart ||:
	fi
	if ! grep -q "search hopebaytech.com" /etc/resolv.conf; then
		echo "search hopebaytech.com" | sudo tee -a /etc/resolv.conf
	fi

	U=${SUDO_USER:-$USER}
	if [[ -n "$U" && "$U" != root && `groups $U` != *docker* ]]; then
		sudo adduser $U docker
		echo To run docker with user, please re-login session
	fi
}

pyhcfs() {
	packages+=" python3-pip"
	export post_pkg_install+=" post_pyhcfs"
}
post_pyhcfs() {
	sudo -H pip3 install --upgrade pip
	sudo -H pip3 install cffi pytest py
}

buildbox() {
	packages+=" rpcbind nfs-common"
	packages+=" zip"
}

install_libzip() {
	TAR_FILE_URL="https://nih.at/libzip/libzip-1.1.3.tar.gz"
	TAR_FILE="${TAR_FILE_URL##*/}"

	BASE_DIR="/tmp"
	SOURCE_DIR="libzip-1.1.3"

	CONF_HEADER_SRC="/usr/local/lib/libzip/include/zipconf.h"
	CONF_HEADER_DEST="/usr/local/include/zipconf.h"

	if [ -f ${CONF_HEADER_DEST} ]
	then
		echo "Libzip already installed.\nSkip installation."
		return
	fi

	pushd ${BASE_DIR}

	echo "Download and extract libzip source..."
	sudo rm -rf ${BASE_DIR}/${SOURCE_DIR}
	sudo rm -rf ${BASE_DIR}/${TAR_FILE}
	wget ${TAR_FILE_URL}
	tar -zxvf ${TAR_FILE}

	echo "Install libzip..."
	cd ${BASE_DIR}/${SOURCE_DIR}
	./configure
	make
	sudo make install
	sudo ln -s ${CONF_HEADER_SRC} ${CONF_HEADER_DEST}

	echo "Clean temp files..."
	sudo rm -rf ${BASE_DIR}/${SOURCE_DIR}
	sudo rm -rf ${BASE_DIR}/${TAR_FILE}

	echo "Libzip installation done!!"
	popd
}

# init for each mode
packages=""
for i in $(echo $setup_dev_env_mode | sed "s/,/ /g")
do
	echo "Setup for $i mode"
	$i
done
source $here/require_compile_deps.bash
install_pkg

# cleanup config file
awk -F'=' '{seen[$1]=$0} END{for (x in seen) print seen[x]}' "$configfile" > /tmp/awk_tmp
sudo mv -f /tmp/awk_tmp "$configfile"

commit_script_changes
commit_script_changes "$configfile"
if ${CI_VERBOSE:-false}; then set -x; else set +x; fi
