#!/bin/bash

# cd /var/cache/apt/archives
# dpkg-scanpackages ./ | gzip > Packages.gz
# export LC_ALL="en_US.UTF-8"
#

LDAP_PACKAGE=`pwd`
HOSTNAME=$(hostname -s)

remove_samba_config(){
	rm -f /etc/samba/smb.conf
}

create_nssldap_config(){
    export DEBIAN_FRONTEND=noninteractive
    export DEBIAN_PRIORITY=critical

    sudo echo "ldap-auth-config ldap-auth-config/ldapns/ldap-server     string  ldapi:///" | debconf-set-selections
    sudo echo "ldap-auth-config ldap-auth-config/ldapns/base-dn string  dc=cosa,dc=com" | debconf-set-selections
    sudo echo "ldap-auth-config ldap-auth-config/ldapns/ldap_version    select  3" | debconf-set-selections
    sudo echo "ldap-auth-config ldap-auth-config/dbrootlogin    boolean true" | debconf-set-selections
    sudo echo "ldap-auth-config ldap-auth-config/dblogin        boolean false" | debconf-set-selections
    sudo echo "ldap-auth-config ldap-auth-config/rootbinddn     string  cn=admin,dc=cosa,dc=com" | debconf-set-selections
    sudo echo "ldap-auth-config ldap-auth-config/rootbindpw password cosa123!" | debconf-set-selections
    sudo echo "nslcd   nslcd/ldap-base string  dc=cosa,dc=com" | debconf-set-selections
    sudo echo "nslcd   nslcd/ldap-uris string  ldapi:///" | debconf-set-selections
    sudo echo "krb5-config krb5-config/add_servers_realm string COSA.LOCAL" | debconf-set-selections
    sudo echo "krb5-config krb5-config/default_realm string COSA.LOCAL" | debconf-set-selections
}

modify_hosts_file(){
    echo $HOSTNAME
    sed -i '/127.0.1.1/d' /etc/hosts
    #echo -e "127.0.1.1\t$HOSTNAME.cosa.com\t$HOSTNAME" >> /etc/hosts
    sed -e "2 i\127.0.1.1	$HOSTNAME.cosa.com	$HOSTNAME" -i /etc/hosts
    
}

install_ldap_package(){
    sudo apt-get install -y --force-yes slapd ldap-utils
}

install_samba_package(){
    sudo apt-get install -y --force-yes samba samba-doc smbldap-tools expect ntp krb5-user winbind
}

replace_smbldap_tools(){
    sudo cp $LDAP_PACKAGE/perl/* /usr/sbin
}

import_samba_schema(){
    sudo ldapadd -Q -Y EXTERNAL -H ldapi:/// -f $LDAP_PACKAGE/cn\=samba.ldif
}

modify_samba_indices(){
    sudo ldapmodify -Q -Y EXTERNAL -H ldapi:/// -f $LDAP_PACKAGE/samba_indices.ldif
}

copy_samba_ldap_config(){
    sudo cp -r $LDAP_PACKAGE/smbldap-tools /etc
}

populate_ldap_object(){
    sudo smbldap-populate "cosa123!"
}

copy_samba_config(){
    sudo cp $LDAP_PACKAGE/smb.conf /etc/samba
}

restart_samba_service(){
    sudo /etc/init.d/smbd restart
    sudo /etc/init.d/nmbd restart
}

set_rootDN_password(){
    sudo smbpasswd -w cosa123!
}

set_locale() {
    echo 'export LC_ALL="en_US.UTF-8"' >> /root/.bashrc
    source /root/.bashrc
}

install_nssldap_package(){
    sudo apt-get install -y --force-yes libnss-ldap
}

copy_nssldap_config(){
    sudo cp $LDAP_PACKAGE/nsswitch.conf /etc 
}

install_nscd_nslcd_package(){
    sudo apt-get install -y --force-yes nscd nslcd
}

restart_nssldap_service(){
    sudo /etc/init.d/nscd restart
    sudo /etc/init.d/nslcd restart
}

copy_exports_file(){
    sudo cp $LDAP_PACKAGE/exports /etc
}

create_admin_user(){
    sudo smbldap-useradd -a -P admin 0wen1sMyL0rd
}

copy_squid3_config(){
    sudo cp $LDAP_PACKAGE/squid.conf /etc/squid3
}

install_winbind(){
    sudo echo "krb5-config krb5-config/add_servers_realm string COSA.LOCAL" | debconf-set-selections
    sudo echo "krb5-config krb5-config/default_realm string COSA.LOCAL" | debconf-set-selections
    apt-get install -y --force-yes expect ntp krb5-user
}




#main
remove_samba_config
create_nssldap_config
modify_hosts_file
install_ldap_package
install_samba_package
replace_smbldap_tools
import_samba_schema
modify_samba_indices
copy_samba_ldap_config
copy_samba_config
populate_ldap_object
restart_samba_service
set_rootDN_password
set_locale
install_nssldap_package
copy_nssldap_config
install_nscd_nslcd_package
restart_nssldap_service
copy_exports_file
create_admin_user
copy_squid3_config
install_winbind
