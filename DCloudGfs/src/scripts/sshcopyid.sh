#!/bin/bash

MANAGEMENT_HOST=$1
[ -z ${MANAGEMENT_HOST} ] && echo " WARN!! ./sshcopyid.sh hostname or ./sshcopyid.sh ipaddress" && exit 1

./sshcopyid.expect ${MANAGEMENT_HOST}
