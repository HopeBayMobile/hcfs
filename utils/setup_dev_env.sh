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
if [[ -f /.dockerinit && "$USER" = jenkins ]]; then
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

case "$setup_dev_env_mode" in
unit_test )
	source $here/require_compile_deps.bash
	packages="$packages gcovr"
	packages="$packages valgrind"
	install_pkg
	;;
functional_test )
	source $here/require_compile_deps.bash
	$here/nopasswd_sudoer.bash
	packages="$packages python-pip python-dev python-swiftclient"
	# generate large file
	packages="$packages openssl units pv"

	install_pkg

	if [ -f $repo/tests/functional_test/requirements.txt ]; then
		sudo -H pip install -q -r $repo/tests/functional_test/requirements.txt
	else
		sudo -H pip install -q -r $here/requirements.txt
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
	;;
docker_host )
	if ! hash docker; then
		install_docker=1
	else
		docker_ver=$(docker version | grep ersion | grep -v API | head -1 | sed s/[^.0-9]//g)
		if dpkg --compare-versions 1.11.2 gt $docker_ver; then
			install_docker=1
		fi
	fi
	if [ ${install_docker:-0} -eq 1 ]; then
		echo "Install Docker"
		packages="$packages wget"
		install_pkg
		sudo apt-key adv --recv-key --keyserver keyserver.ubuntu.com 58118E89F3A912897C070ADBF76221572C52609D
		wget -qO- https://get.docker.com/ > /tmp/install_docker
		sed -i '/$sh_c '\''sleep 3; apt-get update; apt-get install -y -q lxc-docker'\''/c\$sh_c '\''sleep 3; apt-get update; apt-get install -o Dpkg::Options::=--force-confdef -y -q lxc-docker'\''' /tmp/install_docker
		sudo sh /tmp/install_docker
	fi
	if ! grep -q docker:5000 /etc/default/docker; then
		echo "Updating /etc/default/docker"
		echo 'DOCKER_OPTS="$DOCKER_OPTS --insecure-registry docker:5000"' \
			| sudo tee -a /etc/default/docker
		sudo service docker restart ||:
	fi
	install_pkg
	;;
* )
	source $here/require_compile_deps.bash
	install_pkg
	;;
esac

awk -F'=' '{seen[$1]=$0} END{for (x in seen) print seen[x]}' "$configfile" > /tmp/awk_tmp
sudo mv -f /tmp/awk_tmp "$configfile"

commit_script_changes
commit_script_changes "$configfile"
if ${CI_VERBOSE:-false}; then set -x; else set +x; fi
