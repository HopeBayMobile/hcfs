#!/bin/bash

# delete /etc/samba/smb.conf
# Because samba was installed (at savebox) prior to install samba-common 3.6.6, 
#   an interactive screen will pop up to ask something
# in order to eliminate this, we delete it first
rm /etc/samba/smb.conf

# install patch
cd debsrc/

dpkg -i libattr1_2.4.46-8ubuntu1_amd64.deb
dpkg -i libattr1-dev_2.4.46-8ubuntu1_amd64.deb
dpkg -i libacl1_2.2.51-8ubuntu1_amd64.deb
dpkg -i libwbclient0_3.6.6-2ubuntu1_amd64.deb
dpkg -i samba-common_3.6.6-2ubuntu1_all.deb
dpkg -i samba_3.6.6-2ubuntu1_amd64.deb
dpkg -i smbclient_3.6.6-2ubuntu1_amd64.deb
dpkg -i winbind_3.6.6-2ubuntu1_amd64.deb
dpkg -i libpam-smbpass_3.6.6-2ubuntu1_amd64.deb
dpkg -i libpam-winbind_3.6.6-2ubuntu1_amd64.deb

cd ..
