#/bin/bash

S3QLVER='1.10.0003.arkexpress'

# build dependencies
sudo apt-get -y build-dep s3ql
# update change log
dch -b -v ${S3QLVER} -m "Modified by Hope Bay Technologies, Inc."
# build DEB file
dpkg-buildpackage -rfakeroot -b -d

