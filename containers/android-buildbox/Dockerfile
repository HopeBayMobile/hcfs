# Use phusion/baseimage as base image. To make your builds reproducible, make
# sure you lock down to a specific version, not to `latest`!
# See https://github.com/phusion/baseimage-docker/blob/master/Changelog.md for
# a list of version numbers.
FROM phusion/baseimage:0.9.18

# Use baseimage-docker's init system.
CMD ["/sbin/my_init"]

# ...put your own build instructions here...
RUN sed -i 's/archive.ubuntu/tw.archive.ubuntu/g' /etc/apt/sources.list
RUN echo 'Acquire::http::Proxy "http://cache:8000";' > /etc/apt/apt.conf.d/30autoproxy

RUN dpkg --add-architecture i386
RUN apt-get update
RUN apt-get install -y openjdk-7-jdk \
	git-core gnupg flex bison gperf build-essential \
	zip curl gcc-multilib g++-multilib libc6-dev-i386 \
	lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccache \
	libgl1-mesa-dev libxml2-utils xsltproc unzip
RUN apt-get install -y git make python-networkx zlib1g-dev:i386 \
	vim bc libglib2.0-data libsasl2-modules:amd64 gcc g++ \
	syslog-ng-core
RUN apt-get install -y bash-completion
RUN apt-get update && apt-get upgrade -y

ENV USE_CCACHE 1
ENV PATH=/root/bin:$PATH
ADD https://storage.googleapis.com/git-repo-downloads/repo /root/bin/repo
RUN chmod a+x ~/bin/repo
RUN rm -f /etc/service/sshd/down

# Clean up APT when done.
RUN apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*
