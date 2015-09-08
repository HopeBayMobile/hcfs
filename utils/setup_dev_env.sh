#!/bin/bash

# dev dependencies
packages="libattr1-dev"
packages="$packages libfuse-dev"
packages="$packages libcurl4-openssl-dev"
packages="$packages liblz4-dev"     # feature/compress
packages="$packages libssl-dev"

# CI dependencies
packages="$packages gcovr"

packages_to_install=""
for P in $packages; do
    if ! dpkg -s "$P" >/dev/null 2>&1; then
        packages_to_install="$packages_to_install $P"
    fi
done
[ -n "$packages_to_install" ] && sudo apt-get install -y $packages_to_install
