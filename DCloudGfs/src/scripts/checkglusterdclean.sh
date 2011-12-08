#!/bin/bash

PORT=23

./checkglusterdclean.expect 10.7.6.5 $PORT

exit 0

#for i in `seq 1 254`; do
#
#        ./telexpectcheck.expect ${SUBNET1}.${i} $PORT
#
#done

#for i in `seq 1 254`; do
#
#        ./telexpectcheck.expect ${SUBNET2}.${i} $PORT
#
#done

#for i in `seq 1 128`; do
#
#        ./telexpectcheck.expect ${SUBNET3}.${i} $PORT
#
#done
