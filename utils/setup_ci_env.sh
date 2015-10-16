#!/bin/bash
verbose=1
source $WORKSPACE/utils/common_header.bash

WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $here

echo -e "\n======== ${BASH_SOURCE[0]} ========"
echo -e "\n======== ${BASH_SOURCE[0]} should execute with root ========"

# use cache server to speed up docker building
echo 'Acquire::http::Proxy "http://cache:8000";' | tee /etc/apt/apt.conf.d/30autoproxy
export http_proxy="http://cache:8000"
sed -r -i"" "s/archive.ubuntu.com/free.nchc.org.tw/" /etc/apt/sources.list

packages+=" cmake git build-essential"	# Required by oclint / bear
packages+=" openjdk-7-jdk wget unzip"	# Required by PMD for CPD(duplicate code)
packages+=" cloc"						# Install cloc for check code of line
packages+=" mono-complete wget unzip"	# Required mono and CCM for complexity

install_pkg

pushd /
# install BEAR & oclint
if [ ! -d /Bear ]; then
	git clone --depth 1 https://github.com/rizsotto/Bear.git
	pushd Bear
	cmake .
	PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" make all install check package
	popd
fi
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

apt-get upgrade -y
sudo -u jenkins /utils/setup_dev_env.sh -v -m docker_host
sudo -u jenkins /utils/setup_dev_env.sh -v -m functional_test
sudo -u jenkins /utils/setup_dev_env.sh -v -m unit_test
apt-get clean
rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /etc/service/sshd/down
popd
