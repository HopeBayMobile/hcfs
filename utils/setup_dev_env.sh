#!/bin/bash
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
if [[ ${CI:-0} = 1 || -f /.dockerinit && "$USER" = jenkins ]]; then
	sudo chown -R jenkins:jenkins $repo/utils
fi
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
	packages+=" clang-3.5"                 # clang Scan Build Reports
	packages+=" colormake"                 # colorful logs
	packages+=" parallel"                  # parallel check source code style
}
post_static_report() {
	sudo mkdir -p /ci-tools
	sudo chmod 777 /ci-tools
	pushd /ci-tools

	#### Oclint
	if ! bear 2>&1 >/dev/null; then
		git clone --depth 1 https://github.com/rizsotto/Bear.git
		pushd Bear
		cmake .
		make all
		sudo make install
		popd
	fi
	if [ ! -f oclint-0.8.1/bin/oclint ]; then
		wget http://archives.oclint.org/releases/0.8/oclint-0.8.1-x86_64-linux-3.13.0-35-generic.tar.gz
		tar -zxf oclint-0.8.1-x86_64-linux-3.13.0-35-generic.tar.gz
		rm -f oclint-0.8.1-x86_64-linux-3.13.0-35-generic.tar.gz
	fi

	#### Install PMD for CPD(duplicate code)
	if [ ! -d pmd-bin-5.2.2 ]; then
		wget http://downloads.sourceforge.net/project/pmd/pmd/5.2.2/pmd-bin-5.2.2.zip
		unzip pmd-bin-5.2.2.zip
		rm -f pmd-bin-5.2.2.zip
	fi

	#### Install mono and CCM for complexity
	if [ ! -f CCM.exe ]; then
		https_proxy="http://10.0.1.5:8000" \
			wget https://github.com/jonasblunck/ccm/releases/download/v1.1.11/ccm.1.1.11.zip
		unzip ccm.1.1.11.zip
		rm -f ccm.1.1.11.zip
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
	source /etc/lsb-release
	if ! expr `ccache -V 2>&1 | sed -r -n -e "s/.* ([0-9.]+)$/\1/p"` \>= 3.2 2>/dev/null; then
		force_install="$force_install ccache"
	fi
	if [ "$DISTRIB_RELEASE" = "14.04" ] && \
		! expr `apt-cache policy ccache 2>&1 | sed -r -n -e "/Candidate/s/.* ([0-9.]+).*/\1/p"` \>= 3.2 2>/dev/null; then
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
	if hash docker && \
		expr `docker version | sed -n -r -e "/Client/{N;s/[^0-9]*(.*)/\1/p}"` \>= 1.11.2 2>/dev/null; then
		return
	fi

	sudo apt-key adv --recv-key --keyserver keyserver.ubuntu.com 58118E89F3A912897C070ADBF76221572C52609D
	wget -qO- https://get.docker.com/ > /tmp/install_docker
	sed -i '/$sh_c '\''sleep 3; apt-get update; apt-get install -y -q lxc-docker'\''/c\$sh_c '\''sleep 3; apt-get update; apt-get install -o Dpkg::Options::=--force-confdef -y -q lxc-docker'\''' /tmp/install_docker
	sudo sh /tmp/install_docker

	# Setup Docker config
	if ! grep -q docker:5000 /etc/default/docker; then
		echo "Updating /etc/default/docker"
		echo 'DOCKER_OPTS="$DOCKER_OPTS --insecure-registry docker:5000"' \
			| sudo tee -a /etc/default/docker
		sudo service docker restart ||:
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

all() {
	static_report
	unit_test
	functional_test
	docker_host
	install_ccache
	pyhcfs
}

# init for each mode
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
