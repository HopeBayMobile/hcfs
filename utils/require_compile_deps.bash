#!/bin/bash
#########################################################################
#
# Copyright © 2015-2016 Hope Bay Technologies, Inc. All rights reserved.
#
# Abstract:
#
# Revision History
#   2016/1/14 Jethro add copyright.
#
##########################################################################

# dev dependencies
packages+=" build-essential"
packages+=" libattr1-dev"
packages+=" libfuse-dev"
packages+=" libcurl4-openssl-dev"
packages+=" liblz4-dev"
packages+=" libssl-dev"
packages+=" libsqlite3-dev"
packages+=" libjansson-dev"
packages+=" libcap-dev"
echo $packages