#!/bin/bash
echo "Add NOPASSWD for user, required by functional test to replace /etc/hcfs.conf"
if [ -n "$USER" -a "$USER" != "root" -a ! -f /etc/sudoers.d/50_${USER}_sh ]; then
	sudo grep -q "^#includedir.*/etc/sudoers.d" /etc/sudoers || (echo "#includedir /etc/sudoers.d" | sudo tee -a /etc/sudoers)
	( umask 226 && echo "$USER ALL=(ALL) NOPASSWD:ALL" | sudo tee /etc/sudoers.d/50_${USER}_sh )
fi
