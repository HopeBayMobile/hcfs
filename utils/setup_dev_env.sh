#!/bin/bash
set -x
WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && cd .. && pwd )"
here="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $here


configfile="$WORKSPACE/utils/env_config.sh"
if [[ -f /.dockerinit && "$USER" = jenkins ]]; then
	sudo chown -R jenkins:jenkins $WORKSPACE/utils
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

source $WORKSPACE/utils/common_header.bash

setup_status_file="${here}/.setup_$setup_dev_env_mode"
if md5sum --quiet -c "$setup_status_file"; then
	exit
fi
rm -f "$setup_status_file"

if [ $verbose -eq 0 ]; then
	set +x;
else
	echo -e "\n======== ${BASH_SOURCE[0]} mode $setup_dev_env_mode ========"
	set -x;
fi

case "$setup_dev_env_mode" in
unit_test )
	source ./require_compile_deps.bash
	packages="$packages gcovr"
	install_pkg
	;;
functional_test )
	source ./require_compile_deps.bash
	./nopasswd_sudoer.bash
	packages="$packages python-pip python-dev python-swiftclient"
	# generate large file
	packages="$packages openssl units pv"

	install_pkg

	if [ -f $WORKSPACE/tests/functional_test/requirements.txt ]; then
		sudo -H pip install -q -r $WORKSPACE/tests/functional_test/requirements.txt
	else
		sudo -H pip install -q -r /utils/requirements.txt
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
		echo "Install Docker"
		sudo apt-key adv --recv-key --keyserver keyserver.ubuntu.com 58118E89F3A912897C070ADBF76221572C52609D
		curl https://get.docker.com | sudo sh
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
	source ./require_compile_deps.bash
	install_pkg
	;;
esac

awk -F'=' '{seen[$1]=$0} END{for (x in seen) print seen[x]}' "$configfile" > /tmp/awk_tmp
sudo mv -f /tmp/awk_tmp "$configfile"
sudo chmod "$configfile" --reference="${BASH_SOURCE[0]}"
md5sum "${BASH_SOURCE[0]}" "$configfile" | sudo tee "$setup_status_file"
