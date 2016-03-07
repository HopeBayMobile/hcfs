# Use phusion/baseimage as base image. To make your builds reproducible, make
# sure you lock down to a specific version, not to `latest`!
# See https://github.com/phusion/baseimage-docker/blob/master/Changelog.md for
# a list of version numbers.
FROM phusion/baseimage:0.9.18

# Use baseimage-docker's init system.
CMD ["/sbin/my_init"]

# ...put your own build instructions here...
RUN sed -i 's/archive.ubuntu/free.nchc.org.tw/g' /etc/apt/sources.list
RUN echo 'Acquire::http::Proxy "http://10.0.1.5:8000";' > /etc/apt/apt.conf.d/30aptproxy

# Add android repo
ADD https://storage.googleapis.com/git-repo-downloads/repo /root/bin/repo
RUN chmod a+x ~/bin/repo
ENV USE_CCACHE=1 PATH=/root/bin:$PATH

RUN apt-get update \
	    && apt-get install -y aptitude \
	    && aptitude update \
	    && DEBIAN_FRONTEND=noninteractive aptitude full-upgrade -y \
	    && DEBIAN_FRONTEND=noninteractive aptitude install -y openjdk-7-jdk \
	    git-core gnupg flex bison gperf build-essential \
	    zip curl zlib1g-dev gcc-multilib g++-multilib libc6-dev-i386 \
	    lib32ncurses5-dev x11proto-core-dev libx11-dev lib32z-dev ccache \
	    libgl1-mesa-dev libxml2-utils xsltproc unzip \
	    make python-networkx \
	    syslog-ng-core autotools-dev pkg-config libtool vim bc bash-completion \
	    && apt-get clean

RUN echo "if [ -f /data/bashrc ]; then source /data/bashrc; fi" >> /root/.bashrc

# Clean up APT when done.
RUN rm -rf /tmp/* /var/tmp/* /etc/service/sshd/down

VOLUME /data
