FROM phusion/baseimage:0.9.17
MAINTAINER Jethro Yu <jethro.yu@hopebaytech.com>

# Use baseimage-docker's init system.
CMD ["/sbin/my_init"]
ENV PATH="/usr/lib/ccache:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
ENV USE_CCACHE="1"
ENV DEBIAN_FRONTEND=noninteractive
RUN echo 'debconf debconf/frontend select Noninteractive' | debconf-set-selections

RUN adduser --gecos "jenkins" --disabled-password jenkins && \
	echo "jenkins:qwert1234" | chpasswd && \
	echo "jenkins ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	mkdir -p /home/jenkins/.ssh/ && \
	touch /home/jenkins/.ssh/authorized_keys && \
	chown -R jenkins:jenkins /home/jenkins/.ssh && \
	chmod 600 /home/jenkins/.ssh/*

ADD utils/ /utils/

# CI runs build/test in all roles
RUN /utils/setup_ci_env.sh
