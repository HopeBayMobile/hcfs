#! /bin/bash
# This script is used to bundle packages which are required by ZCW.
# Delta Electronics, Inc. 2012

### 0. Define variables
THISPATH=$(pwd)
BASEPATH=${THISPATH%%/deploy}
ZONEPATH=${BASEPATH}/ZONES

### 1. Bundle required Packages
PACKAGEPATH=${THISPATH}/pkg
mkdir ${PACKAGEPATH}

cd ${PACKAGEPATH}
#Update APT package index
#apt-get -o APT::Architecture="amd64" update
apt-get update

## Download required packages
#apt-get -o APT::Architecture="amd64" download $(cut -f1 pkg.require)
apt-get download $(cut -f1 ${THISPATH}/pkg.require)

#Find dependency package list
pkglist=$(apt-cache depends $(cut -f1 ${THISPATH}/pkg.require) | grep "Depends:" | tr -d "|<> " | sort | uniq)
## Download each dependency
for pkg in ${pkglist}
do
	#printf "%s\n" ${pkg#"Depends:"}
	#apt-get -o APT::Architecture="amd64" download ${pkg#"Depends:"}
	apt-get download ${pkg#"Depends:"}
done

apt-ftparchive packages ./ > ./Packages

### 2. Bundle required Python packages
cd ${PACKAGEPATH}
if ! pip bundle ZCW.pybundle -r ${THISPATH}/pip.require; then
	echo "Bundling Python packages failure."
	exit 1
fi

rm -r build-bundle/

### Finish bundling packages
exit 0
