#!/bin/bash
## this script is used at BAC0090 for forwarding 80 port to a lxc container
iptables -t nat -F	# clean all rules

iptables -t nat -A PREROUTING -i eth0 -p tcp -d 61.60.144.196 --dport 80 -j DNAT --to-destination 10.0.3.117:80

# iptables -t nat -A POSTROUTING -s 10.0.3.0/24 -o eth0 -j SNAT --to 61.60.144.196
