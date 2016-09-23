FROM phusion/baseimage:0.9.18
MAINTAINER Jethro Yu <jethro.yu@hopebaytech.com>

# Use baseimage-docker's init system.
CMD ["/sbin/my_init"]

#
# Common settings
#
# Ebable SSHD
RUN rm /etc/service/sshd/down
# APT Setting
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections
# APT Speedup
RUN sed -i 's/archive.ubuntu.com/10.0.1.5:8001/g' /etc/apt/sources.list
# Time zone setup
ENV TZ=Asia/Taipei
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
ENV PATH="/usr/lib/ccache:$PATH" \
	USE_CCACHE=1 \
	TERM=xterm \
	DEBIAN_FRONTEND=noninteractive \
	HOME=/root

#
# Main image content
#
ADD internal/root/ /root/

# Required google repo tool
RUN curl https://storage.googleapis.com/git-repo-downloads/repo > /usr/bin/repo && chmod a+x /usr/bin/repo

RUN export AOSP="openjdk-7-jdk git-core gnupg flex bison gperf build-essential zip \
	curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 \
	lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccache \
	libgl1-mesa-dev libxml2-utils xsltproc unzip make python-networkx \
	syslog-ng-core autotools-dev pkg-config libtool" \
	BASIC="vim bc bash-completion" &&\
	apt update && apt upgrade -y &&\
	apt install -y $AOSP $BASIC &&\
	apt-get clean && rm -rf /tmp/* /var/tmp/*

# Gitlab
RUN ssh-keygen -R gitlab.hopebaytech.com &&\
	ssh-keygen -R 10.10.99.200 &&\
	ssh-keygen -R gitlab.hopebaytech.com,10.10.99.200 &&\
	ssh-keyscan -H gitlab.hopebaytech.com,10.10.99.200 >> ~/.ssh/known_hosts &&\
	ssh-keyscan -H 10.10.99.200 >> ~/.ssh/known_hosts &&\
	ssh-keyscan -H gitlab.hopebaytech.com >> ~/.ssh/known_hosts
