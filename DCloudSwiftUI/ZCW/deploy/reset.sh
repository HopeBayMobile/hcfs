#! /bin/bash
# This script is used to remove what deployment install.
# Delta Electronics, Inc. 2012

### 1. Uninstall required services
#Stop service
apache2ctl-zcw stop
update-rc.d -f apache2-zcw remove
apt-get -y remove apache2 apache2-utils

/etc/init.d/celeryd stop
update-rc.d -f celeryd remove
rm /etc/init.d/celeryd
rm /etc/default/celeryd

### 2. Uninstall required Python packages
pip uninstall -y -r pip.require

#uninstall Delta-developed Pyhton packages
pip uninstall -y delta_wizard
pip uninstall -y delta_jsonapi
pip uninstall -y delta_form

#remove pip finally
apt-get -y remove python-pip

### 3. Remove ZCW Configuration
rm -r /etc/apache2-zcw
rm -r /etc/init.d/apache2-zcw
rm /usr/local/sbin/*-zcw
rm /etc/logrotate.d/apache2-zcw
rm -r /var/log/apache2-zcw


### 4. Remove deployed Django Project
rm -r /var/www-zcw

### Remove zcw command from bin path
rm /usr/local/bin/zcw
