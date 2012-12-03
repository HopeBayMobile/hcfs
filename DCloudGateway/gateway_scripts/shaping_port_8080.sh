echo "Configuring NIC:" $1
echo "Set bandwidth limit of port 8080 = " $2
# clean tc rules
tc qdisc del dev $1 root  # clean tc settings
# flush iptables settings
#iptables -F
#iptables -X
#iptables -t nat -F
#iptables -t nat -X
iptables -t mangle -F
iptables -t mangle -X
iptables -P INPUT ACCEPT
iptables -P FORWARD ACCEPT
iptables -P OUTPUT ACCEPT

# set tc classes
tc qdisc add dev $1 parent root handle 1: hfsc default 11
tc class add dev $1 parent 1: classid 1:1 hfsc sc rate 1024mbps ul rate 1024mbps
tc class add dev $1 parent 1:1 classid 1:11 hfsc sc rate 1024mbps ul rate 1024mbps
tc class add dev $1 parent 1:1 classid 1:12 hfsc sc rate $2kbps ul rate $2kbps

tc qdisc add dev $1 parent 1:11 handle 11:1 pfifo
tc qdisc add dev $1 parent 1:12 handle 12:1 pfifo
# map port 8080 to the slow calss
iptables -A OUTPUT -t mangle -p tcp --dport 8080 -j CLASSIFY --set-class 1:12
