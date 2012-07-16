#!/bin/sh

# another way to use ethtool to check port interface
#if ethtool eth0 | grep -q "Port: MII";
# eth0 should have IRQ 16
# eth1 should have IRQ 23
if ifconfig -a eth0 | grep -q "Interrupt:23"
then
   # Misdetected eth0 and eth1
   echo "Misordered NICs. Reordering them."
   ifdown eth0
   ifdown eth1
   ip link set dev eth0 name not_eth0
   ip link set dev eth1 name eth0
   ip link set dev not_eth0 name eth1
   echo "Removing udev rules"
   rm -rf /etc/udev/rules.d/70-persistent-net.rules
   /etc/init.d/networking restart
else
   # Correct eth order
   echo "Order of NICs is correct."
fi
