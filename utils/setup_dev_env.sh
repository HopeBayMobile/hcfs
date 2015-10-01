#!/bin/bash

WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
configfile="$WORKSPACE/utils/env_config.sh"
touch "$configfile"

. $WORKSPACE/utils/trace_error.bash
set -e
set +x # Pause debug, enable if verbose on
[[ "$flags" =~ "x" ]] && flag_x="-x" || flag_x="+x"

# A POSIX variable
OPTIND=1         # Reset in case getopts has been used previously in the shell.

# Initialize our own variables:
output_file=""
verbose=${verbose-0}

while getopts ":vm:" opt; do
	case $opt in
	m)
		setup_dev_env_mode=$OPTARG
		;;
	v)
		verbose=1
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

setup_status_file="${here}/.setup_$setup_dev_env_mode"
if [ -f $setup_status_file ]l then
	sudo chmod a+w "$setup_status_file"
fi
if md5sum --quiet -c "$setup_status_file"; then
	exit
fi

echo -e "\n======== ${BASH_SOURCE[0]} mode $setup_dev_env_mode ========"

if [ $verbose -eq 0 ]; then set +x; else set -x; fi

function install_pkg (){
	set +x
	for pkg in $packages;
	do
		if ! dpkg -s $pkg >/dev/null 2>&1; then
			install="$install $pkg"
		fi
	done
	if [ $verbose -eq 0 ]; then set +x; else set -x; fi
	if [ "$install $force_install" != " " ]; then
		sudo apt-get install -y $install $force_install
		packages=""
		install=""
		force_install=""
	fi
}

# Add NOPASSWD for user, required by functional test to replace /etc/hcfs.conf
if [ -n "$USER" -a "$USER" != "root" -a ! -f /etc/sudoers.d/50_${USER}_sh ]; then
	sudo grep -q "^#includedir.*/etc/sudoers.d" /etc/sudoers || (echo "#includedir /etc/sudoers.d" | sudo tee -a /etc/sudoers)
	( umask 226 && echo "$USER ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/50_${USER}_sh )
fi

case "$setup_dev_env_mode" in
docker_slave )
	echo 'Acquire::http::Proxy "http://cache:8000";' | sudo tee /etc/apt/apt.conf.d/30autoproxy
	export http_proxy="http://cache:8000"
	echo export "http_proxy=\"http://cache:8000\"" >> "$configfile"
	sudo sed -r -i"" "s/archive.ubuntu.com/free.nchc.org.tw/" /etc/apt/sources.list
	packages="$packages cmake git"					# Required by oclint / bear
	packages="$packages openjdk-7-jdk wget unzip"	# Required by PMD for CPD(duplicate code)
	packages="$packages cloc"						# Install cloc for check code of line
	packages="$packages mono-complete wget unzip"	# Required mono and CCM for complexity
	;;&
docker_slave | unit_test | functional_test )
	# dev dependencies
	packages="$packages\
	build-essential \
	libattr1-dev \
	libfuse-dev \
	libcurl4-openssl-dev \
	liblz4-dev \
	libssl-dev \
	"
	# Use ccache to speedup compile
	if ! echo $PATH | grep -E "(^|:)/usr/lib/ccache(:|$)"; then
		echo "export PATH=\"/usr/lib/ccache:$PATH\"" >> "$configfile"
	fi
	if ! ccache -V | grep 3.2; then
		if ! apt-cache policy ccache | grep 3.2; then
			source /etc/lsb-release
			echo "deb http://ppa.launchpad.net/comet-jc/ppa/ubuntu $DISTRIB_CODENAME main" \
				| sudo tee /etc/apt/sources.list.d/comet-jc-ppa-trusty.list
			sudo apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 32EF5841642ADD17
			sudo apt-get update
		fi
		force_install="$force_install ccache"
	fi
	echo "export USE_CCACHE=1" >> "$configfile"
	;;&
docker_slave | unit_test )
	packages="$packages gcovr"
	;;&
docker_slave | functional_test )
	packages="$packages python-pip python-dev python-swiftclient"
	# generate large file
	packages="$packages openssl units pv"
	;;&
docker_slave | docker_host )
	# Install Docker
	if ! hash docker; then
		curl https://get.docker.com | sudo sh
	fi
	if ! grep -q docker:5000 /etc/default/docker; then
		echo 'DOCKER_OPTS="$DOCKER_OPTS --insecure-registry docker:5000"' \
			| sudo tee -a /etc/default/docker
		CHANGED_DOCKER_SETTING=1
	fi
	;;&
docker_host )
	if [[ $CHANGED_DOCKER_SETTING == 1 ]]; then
		sudo service docker restart
	fi
	;;&
esac

install_pkg

# Post-install
case "$setup_dev_env_mode" in
docker_slave | functional_test )
	if [ -f $WORKSPACE/tests/functional_test/requirements.txt ]; then
		sudo -H pip install -q -r $WORKSPACE/tests/functional_test/requirements.txt
	else
		sudo -H pip install -q -r requirements.txt
	fi
	echo "########## Configure user_allow_other in /etc/fuse.conf"
	if sudo grep "#user_allow_other" /etc/fuse.conf; then
		sudo sed -ir -e "s/#user_allow_other/user_allow_other/" /etc/fuse.conf
	fi
	if [[ -n "$USER" && "$USER" != root && `groups $USER` != *fuse* ]]; then
		sudo addgroup "$USER" fuse
	fi
	if [[ `groups jenkins` != *fuse* ]]; then
		sudo addgroup jenkins fuse || :
	fi
	;;&
docker_slave )
	pushd /
	# install BEAR
	if [ ! -d Bear ]; then
		git clone --depth 1 https://github.com/rizsotto/Bear.git
		pushd Bear
		cmake .
		PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" make all install check package
		popd
	fi
	# Install oclint
	if [ ! -d /oclint-0.8.1 ]; then
		wget http://archives.oclint.org/releases/0.8/oclint-0.8.1-x86_64-linux-3.13.0-35-generic.tar.gz
		tar -zxf oclint-0.8.1-x86_64-linux-3.13.0-35-generic.tar.gz
		rm -f oclint-0.8.1-x86_64-linux-3.13.0-35-generic.tar.gz
	fi

	# Install PMD for CPD(duplicate code)
	if [ ! -d /pmd-bin-5.2.2 ]; then
		wget http://downloads.sourceforge.net/project/pmd/pmd/5.2.2/pmd-bin-5.2.2.zip
		unzip pmd-bin-5.2.2.zip
		rm -f pmd-bin-5.2.2.zip
	fi

	# Install mono and CCM for complexity
	if [ ! -f /CCM.exe ]; then
		wget https://github.com/jonasblunck/ccm/releases/download/v1.1.7/ccm_binaries.zip
		unzip ccm_binaries.zip
		rm -f ccm_binaries.zip
	fi

	# Cleanup image
	sudo apt-get clean
	sudo rm -rf /var/lib/apt/lists/*
	popd
	;;
esac

awk -F'=' '{seen[$1]=$0} END{for (x in seen) print seen[x]}' "$configfile" > awk_tmp
mv -f awk_tmp "$configfile"

md5sum --tag "${BASH_SOURCE[0]}" "$configfile" > "$setup_status_file"
set $flag_x
