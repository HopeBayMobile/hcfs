#! /bin/bash
# This script is used to deploy Zone Config Wizard (ZCW).
# Delta Electronics, Inc. 2012

### 0. Define variables
THISPATH=$(pwd)
BASEPATH=${THISPATH%%/deploy}
#ZONEPATH=${BASEPATH}/ZONES
ZONEPATH=${BASEPATH}
SUFFIX=zcw

if [ $# -ne 1 ]; then
	echo 'Deployed Django project need to be assigned.'
	exit 1
fi

PROJECTNAME=$1
PROJECTPATH=${ZONEPATH}/${PROJECTNAME}

if [ ! -d ${PROJECTPATH} ]; then
	echo "Django project \"$1\" not found in \"${ZONEPATH}\""
	exit 1
fi

### 3. Configure ZCW
#Turn off Apache Daemon
update-rc.d -f apache2 remove
apache2ctl-zcw stop
# wait for apache2 to stop
sleep 5

### 4. Setup ZCW
cd ${THISPATH}

if [ -e /etc/apache2-$SUFFIX ] ; then
	echo "/etc/apache2-$SUFFIX already exists"
	echo "remove /etc/apache2-$SUFFIX"
	rm -rf /etc/apache2-$SUFFIX
        rm -rf /usr/local/sbin/*-$SUFFIX
        rm -rf /var/log/apache2-$SUFFIX
        rm -rf /var/www-$SUFFIX
fi
## 4.1 Setup Apache HTTP Server
#Execute setup-instance command
source /usr/share/doc/apache2/examples/setup-instance ${SUFFIX}

#Enable wsgi
a2enmod-zcw wsgi
#Copy Apache configurations
cp -r apache2-zcw/ /etc/
echo "PROJECTNAME=${PROJECTNAME}"

#Replace PROJECTNAME variable
sed -i "s/PROJECTNAME/${PROJECTNAME}/g" /etc/apache2-zcw/sites-available/default

#Replace static alias directory
sed -i "s/\/var\/www-${SUFFIX}\/static\//\/var\/www-${SUFFIX}\/${PROJECTNAME}\/${PROJECTNAME}\/static/g" /etc/apache2-zcw/sites-available/default

#Copy Django project
mkdir /var/www-zcw
cp -r ${PROJECTPATH}/ /var/www-zcw/
#Collects the static files into STATIC_ROOT
python /var/www-zcw/${PROJECTNAME}/manage.py collectstatic --noinput


#FIXME: 
cp -r /usr/local/lib/python2.7/dist-packages/delta_wizard-0.1-py2.7.egg/delta/wizard/static/wizard /var/www-zcw/${PROJECTNAME}/${PROJECTNAME}/static

#change file owner and group
chown www-data:www-data -R /var/www-zcw/

## 4.2 Setup Celery Task Queue
#Copy Celery configurations
cp -r celeryd/* /
#Replace PROJECTNAME variable
sed -i "s/PROJECTNAME/$PROJECTNAME/g" /etc/default/celeryd

### 5. Start service
#Note: ZCW does not start after deploying. 
#This should be started by "start_script/zcw_init.py"!

### Copy zcw command to bin path
cp zcw /usr/local/bin/zcw
chmod a+x /usr/local/bin/zcw

cd ${BASEPATH}
cp start_script/*.py /usr/local/bin/

#not safe
#sed -i "s/exit 0/python \/usr\/local\/bin\/zcw_init\.py \nexit 0/g" /etc/rc.local

### Finish deploying ZCW
echo "Finish deploying ZCW"
exit 0
