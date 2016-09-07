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
	DEBIAN_FRONTEND=noninteractive

#
# Main image content
#

# Jenkins slave user
ADD authorized_keys id_rsa /home/jenkins/.ssh/id_rsa
RUN adduser --gecos "jenkins" --disabled-password jenkins && \
	echo "jenkins:qwert1234" | chpasswd && \
	echo "jenkins ALL=(ALL) NOPASSWD:ALL" >> /etc/sudoers && \
	chown -R jenkins:jenkins /home/jenkins && \
	chmod 600 /home/jenkins/.ssh/*


# Setup NDK
WORKDIR /opt
RUN wget -N ftp://172.16.10.200/ubuntu/CloudDataSolution/HCFS_android/resources/android-ndk-r10e-linux-x86_64.bin && \
    chmod a+x android-ndk-r10e-linux-x86_64.bin &&\
    ./android-ndk-r10e-linux-x86_64.bin &&\
    rm ./android-ndk-r10e-linux-x86_64.bin &&\
    chmod -R a+rwX ./android-ndk-r10e
ENV PATH=/opt/android-ndk-r10e/:$PATH
WORKDIR /
# CI runs build/test in all roles
# utils/ is prepared in build.sh
ADD utils/ /build/utils/
RUN chown -R jenkins. /build/utils/
USER jenkins
RUN CI=1 /build/utils/setup_dev_env.sh -m static_report
RUN CI=1 /build/utils/setup_dev_env.sh -m unit_test
RUN CI=1 /build/utils/setup_dev_env.sh -m functional_test
RUN CI=1 /build/utils/setup_dev_env.sh -m docker_host
RUN CI=1 /build/utils/setup_dev_env.sh -m install_ccache
RUN CI=1 /build/utils/setup_dev_env.sh -m pyhcfs
RUN sudo apt upgrade -y && sudo rm -rf /tmp/* /var/tmp/*
