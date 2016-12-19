#!/bin/bash
#########################################################################
#
# Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#
# Revision History
#   2016/1/14 Jethro add copyright.
#
##########################################################################

# dev dependencies
packages+=" build-essential"
packages+=" libattr1-dev"
packages+=" libfuse-dev"
packages+=" libcurl4-openssl-dev"
packages+=" liblz4-dev"
packages+=" libssl-dev"
packages+=" libsqlite3-dev"
packages+=" libjansson-dev"
packages+=" libcap-dev"
#packages+=" python3-dev python3-setuptools python3-pip"

# libzip
packages+=" zlib1g-dev"
packages+=" wget"
export post_pkg_install+=" install_libzip"

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
