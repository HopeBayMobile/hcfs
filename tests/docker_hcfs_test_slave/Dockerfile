FROM phusion/baseimage:0.9.17
MAINTAINER Jethro Yu <jethro.yu@hopebaytech.com>

# Use baseimage-docker's init system.
CMD ["/sbin/my_init"]
ENV PATH "/usr/lib/ccache:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

RUN adduser --gecos "jenkins" --disabled-password jenkins && \
	echo "jenkins:qwert1234" | chpasswd && \
	echo "jenkins ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	mkdir -p /home/jenkins/.ssh/ && \
	touch /home/jenkins/.ssh/authorized_keys && \
	chown -R jenkins:jenkins /home/jenkins/.ssh && \
	chmod 600 /home/jenkins/.ssh/*

ADD utils/ /utils/
RUN /utils/setup_dev_env.sh -v -m docker_slave && \
	apt-get upgrade -y && \
	apt-get clean && \
	rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/* /etc/service/sshd/down
