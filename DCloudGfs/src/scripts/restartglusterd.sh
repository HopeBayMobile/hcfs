#!/bin/bash

#sh d.sh &> log

#grep "glusterd is alive" log | awk ' { print $1 }' > iplist
IPLIST=`cat iplist`
CMD="$1"


for i in ${IPLIST}; do

        ./removeglusterd.expect

done

for i in ${IPLIST}; do

        ./startglusterd.expect

done
