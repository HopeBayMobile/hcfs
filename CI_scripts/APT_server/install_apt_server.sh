#!/bin/bash

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# install packages
apt-get install -y gnupg rng-tools reprepro apache2 expect
# configure randomness generator
echo "HRNGDEVICE=/dev/urandom" >> /etc/default/rng-tools
/etc/init.d/rng-tools start

# generate gpg key
./gpg_key_gen.sh

# configure reprepro
mkdir -p /var/packages/ubuntu/conf
cat > /var/packages/ubuntu/conf/distributions << EOF
Origin: Delta
Label: Delta Cloud Data Team
Codename: natty
Architectures: amd64 source
Components: main
Description: Operated by Delta Cloud Data Team
SignWith: yes
DebOverride: ../override/override.natty
DscOverride: ../override/override.natty

Origin: Delta
Label: Delta Cloud Data Team
Codename: precise
Architectures: amd64 source
Components: main
Description: Operated by Delta Cloud Data Team
SignWith: yes
DebOverride: ../override/override.precise
DscOverride: ../override/override.precise

EOF

mkdir -p
touch /var/packages/ubuntu/override/override.natty
touch /var/packages/ubuntu/override/override.precise

cat > /var/packages/ubuntu/conf/options << EOF
verbose
ask-passphrase
basedir .
EOF

ln -s /var/packages/ /var/www/
