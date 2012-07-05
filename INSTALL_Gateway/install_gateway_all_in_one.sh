#!/bin/bash
# install_gateway_all_in_one.sh : will install S3ql, GUI api and GUI all in once.

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# install dependent packages via apt-get
apt-get install -y --force-yes python-pycryptopp libsqlite3-dev sqlite3 python-apsw python-lzma 
apt-get install -y --force-yes libfuse-dev libattr1-dev python-all-dev python-sphinx build-essential 
apt-get install -y --force-yes python-profiler python-argparse fuse-utils 
apt-get install -y --force-yes python-pip python-setuptools python-profiler
apt-get install -y --force-yes python-software-properties curl portmap nfs-kernel-server samba
add-apt-repository ppa:swift-core/release
apt-get update
apt-get install -y --force-yes swift apt-show-versions squid3
    # install apache and mod_wsgi
apt-get install -y --force-yes apache2
apt-get install -y --force-yes libapache2-mod-wsgi


# vvvvv-- install S3QL and its dependent packages ---------------------------------------------------
dpkg -i ../DCloudS3ql/debsrc/python-llfuse_0.37.1-2_amd64.deb 
dpkg -i ../DCloudS3ql/debsrc/cython_0.15.1-2_amd64.deb 
dpkg -i ../DCloudS3ql/debsrc/smartmontools_5.39.1+svn3124-2_amd64.deb
dpkg -i ../DCloudS3ql/debsrc/s3ql_1.12.0~natty1_amd64.deb
dpkg -i ../DCloudGateway/sudo_1.8.5-2_amd64.deb

    # create soft link for s3ql program
cp -rs /usr/bin/*s3ql* /usr/local/bin/
# ^^^^^-- install S3QL and its dependent packages ----------------------------------------------------

# vvvvv-- install GUI --------------------------------------------------------------------------------
cd ../DCloudGatewayUI
THISPATH=$(pwd)
BASEPATH=${THISPATH%%/deploy}
BASENAME=$(/usr/bin/basename $BASEPATH)

# Check the root directory name
if [ "$BASENAME" != "DCloudGatewayUI" ]; then
   echo "Delta Cloud Storage Gateway UI root path not properly named!"
   echo "should be 'DCloudGatewayUI'"
   exit 1
fi

# Check if the installation package listing exist
if [ ! -s pip.require ]; then
   echo "pip.require file not found!"
   exit 1
fi

echo "Install required python packages..."
# Install required python modules
pip install $BASEPATH/deploy/externals/amqplib-1.0.2.tgz
pip install $BASEPATH/deploy/externals/anyjson-0.3.1.tar.gz
pip install $BASEPATH/deploy/externals/celery-2.5.3.tar.gz
pip install $BASEPATH/deploy/externals/Django-1.4.tar.gz
pip install $BASEPATH/deploy/externals/django-celery-2.5.5.tar.gz
pip install $BASEPATH/deploy/externals/django-picklefield-0.2.1.tar.gz
pip install $BASEPATH/deploy/externals/kombu-2.1.6.tar.gz
pip install $BASEPATH/deploy/externals/ordereddict-1.1.tar.gz
pip install $BASEPATH/deploy/externals/python-dateutil-1.5.tar.gz
pip install $BASEPATH/deploy/externals/SQLAlchemy-0.7.7.tar.gz

# Close the debug mode of PDCM when deploy
sed -e "s,DEBUG = True,DEBUG = False,g" -ie $BASEPATH/GatewayUI/settings.py

# Create the log file
LOGFOLDER=$BASEPATH/lib/logs
mkdir -p $LOGFOLDER
touch $LOGFOLDER/errors
chown -R www-data:www-data $LOGFOLDER

# enable mod_wsgi
a2enmod wsgi

# Copy Delta Cloud Manager Django app to /var/www
cp -f -r $BASEPATH /var/www/

# Add Delta Cloud Manager .conf to apache2, skip if already exists
INCLUDE_CONF="Include /var/www/$BASENAME/deploy/apache/dcloud.conf"
grep $INCLUDE_CONF /etc/apache2/apache2.conf > /dev/null
if [ "$?" -ne "1" ]; then
   echo "Include /var/www/$BASENAME/deploy/apache/dcloud.conf" >> /etc/apache2/apache2.conf
fi

/etc/init.d/apache2 restart

#deploy Celery Task Queue
cp -f $BASEPATH/deploy/celeryd/etc/init.d/celeryd /etc/init.d/celeryd
cp -f $BASEPATH/deploy/celeryd/etc/default/celeryd /etc/default/celeryd

chmod 755  /etc/init.d/celeryd
/etc/init.d/celeryd restart

# initialize django database stuff
python /var/www/$BASENAME/manage.py syncdb --noinput

sleep 3

mkdir -p /var/log/delta
chown www-data:www-data -R /var/www/$BASENAME/GatewayUI/
chown www-data:www-data -R /var/log/delta

echo "DCloudGatewayUI setup completed..."
# ^^^^^-- install GUI --------------------------------------------------------------------------------


# vvvvv-- install GUI API -----------------------------------------------------------------------------
cd ../DCloudGateway
python setup.py install
    # install squid3 proxy
CACHEDIR="/storage/http_proxy_cache/"
mkdir -p $CACHEDIR
cat > /etc/squid3/squid.conf << EOF
http_port 3128 transparent
acl localnet src 127.0.0.1/255.255.255.255
http_access allow all
http_access allow localnet
dns_nameservers 8.8.8.8  168.95.1.1

# cache_dir ufs <cache-dir> <cache size in MB> <# of L1 directories> <# of L2 directories>
cache_dir ufs $CACHEDIR 51200 64 128
EOF

chmod 777 $CACHEDIR

# add double check cache directory at each power on
cat >/etc/init.d/make_http_proxy_cache_dir <<EOF
mkdir $CACHEDIR
chmod 777 $CACHEDIR
EOF
chmod 777 /etc/init.d/make_http_proxy_cache_dir
cp -rs /etc/init.d/make_http_proxy_cache_dir /etc/rc2.d/S29make_http_proxy_cache_dir

echo "    Squid3 configuration has been written."
# ^^^^^-- install GUI API -----------------------------------------------------------------------------


# add a private of APT repository server
cat >/etc/apt/sources.list.d/delta-server.list <<EOF
deb http://172.16.229.87/packages/ubuntu/ natty main
deb-src http://172.16.229.87/packages/ubuntu natty main
EOF

sleep 3
echo "Installation of Gateway is completed..."
