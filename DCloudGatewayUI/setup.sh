#!/bin/sh

# Make sure only root can run this script
if [ "$(id -u)" -ne "0" ]; then
   echo "This script must be run as root, use 'sudo'" 1>&2
   exit 1
fi

# Find the root directory name
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

# install apache
apt-get install -y --force-yes apache2

# Copy Delta Cloud Manager Django app to /var/www
cp -r $BASEPATH /var/www/$BASENAME

# Add Delta Cloud Manager .conf to apache2
echo "Include /var/www/$BASENAME/deploy/apache/dcloud.conf" >> /etc/apache2/apache2.conf

/etc/init.d/apache2 restart

# initialize django database stuff
python /var/www/$BASENAME/manage.py syncdb --noinput

cp /var/www/$BASENAME/deploy/celeryd/etc/init.d/celeryd /etc/init.d/celeryd
cp /var/www/$BASENAME/deploy/celeryd/etc/default/celeryd /etc/default/celeryd

/etc/init.d/celeryd start

echo "DCloudGatewayUI setup completed..."
