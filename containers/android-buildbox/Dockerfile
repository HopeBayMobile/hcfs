# Use phusion/baseimage as base image. To make your builds reproducible, make
# sure you lock down to a specific version, not to `latest`!
# See https://github.com/phusion/baseimage-docker/blob/master/Changelog.md for
# a list of version numbers.
FROM phusion/baseimage:0.9.18

# Use baseimage-docker's init system.
CMD ["/sbin/my_init"]

# ...put your own build instructions here...
ENV USE_CCACHE=1 PATH=/root/bin:$PATH TERM=xterm
ADD internal/root/ /root/

ADD internal/00-apt-config /internal/00-apt-config
RUN /internal/00-apt-config

ADD internal/01-add-google-repo /internal/01-add-google-repo
RUN /internal/01-add-google-repo

ADD internal/02-install-pkgs /internal/02-install-pkgs
RUN /internal/02-install-pkgs

ADD internal/03-ssh-known-hosts /internal/03-ssh-known-hosts
RUN /internal/03-ssh-known-hosts

RUN rm /etc/service/sshd/down
