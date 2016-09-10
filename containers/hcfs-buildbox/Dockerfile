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
ENV TERM=xterm \
	DEBIAN_FRONTEND=noninteractive

#
# Main image content
#

# Jenkins slave user
RUN adduser --gecos "jenkins" --disabled-password jenkins && \
	echo "jenkins:qwert1234" | chpasswd && \
	echo "jenkins ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers
ADD internal.tgz /
# For submodule pulling from gitlab
COPY id_rsa /home/jenkins/.ssh/
RUN chown -R jenkins. /utils /home/jenkins && \
	chmod 600 /home/jenkins/.ssh/*

USER jenkins
RUN CI=1 /utils/setup_dev_env.sh -m static_report
RUN CI=1 /utils/setup_dev_env.sh -m unit_test
RUN CI=1 /utils/setup_dev_env.sh -m functional_test
RUN CI=1 /utils/setup_dev_env.sh -m docker_host
RUN CI=1 /utils/setup_dev_env.sh -m install_ccache
ENV USE_CCACHE=1 PATH="/usr/lib/ccache:$PATH"
RUN CI=1 /utils/setup_dev_env.sh -m pyhcfs
RUN sudo apt upgrade -y && sudo rm -rf /tmp/* /var/tmp/*

# Setup NDK
USER root
WORKDIR /opt
RUN umask 000 &&\
	URL=ftp://172.16.10.200/ubuntu/CloudDataSolution/HCFS_android/resources/android-ndk-r12b-linux-x86_64.zip && \
	wget -N $URL && unzip "${URL##*/}" && rm "${URL##*/}"
ENV PATH=/opt/android-ndk-r12b/:$PATH

WORKDIR /home/jenkins
EXPOSE 22
