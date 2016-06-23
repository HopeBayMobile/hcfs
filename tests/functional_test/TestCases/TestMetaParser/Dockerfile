FROM docker:5000/ubuntu:14.04
MAINTAINER Ted Chen <ted.chen@hopebaytech.com>

# Use baseimage-docker's init system.

ADD hcfs /hcfs
RUN apt-get -y update
RUN apt-get -y install python
RUN apt-get -y install python-swiftclient
RUN /bin/sh -c /hcfs/utils/setup_dev_env.sh -m functional_test
RUN /bin/sh -c /hcfs/utils/setup_dev_env.sh -m docker_host



