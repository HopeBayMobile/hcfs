#!/bin/bash

#for i in `seq 1 80`; do
#for i in `seq 2 80`; do
for i in `seq 11 80`; do

if [ ${i} -le 9 ]; then
	ssh ntu0${i} "echo ntu0${i}; sudo mkfs.ext4 /dev/sdb ; sudo mkdir -p /export1 ; sudo mkdir -p /export2 ; sudo mount /dev/sdb /export2"
	./mkfs.expect ntu0${i}
	ssh ntu0${i} "sudo mkdir -p /export1 ; sudo mkdir -p /export2 ; sudo mount /dev/sdb /export2"
	#ssh ntu0${i} "sudo mkdir -p /export1/brick1 ; sudo mkdir -p /export2/expect2 "
else
	./mkfs.expect ntu${i}
	#ssh ntu${i} "echo ntu${i}; sudo mkfs.ext4 /dev/sdb ; sudo mkdir -p /export1 ; sudo mkdir -p /export2 ; sudo mount /dev/sdb /export2"
	ssh ntu${i} "sudo mkdir -p /export1 ; sudo mkdir -p /export2 ; sudo mount /dev/sdb /export2"
	#ssh ntu${i} "sudo mkdir -p /export1/brick1 ; sudo mkdir -p /export2/expect2 "
fi
done
