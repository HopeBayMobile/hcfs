# Use phusion/baseimage as base image. To make your builds reproducible, make
# sure you lock down to a specific version, not to `latest`!
# See https://github.com/phusion/baseimage-docker/blob/master/Changelog.md for
# a list of version numbers.
FROM docker:5000/android-buildbox

# Use baseimage-docker's init system.
#CMD ["/sbin/my_init"]

ADD internal/data/ /data/

WORKDIR /data
RUN export TERM=c && git fetch --progress --verbose hb master && git checkout master
