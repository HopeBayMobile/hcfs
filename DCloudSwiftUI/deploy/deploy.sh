#! /bin/bash
# This script is used to deploy Zone Config Wizard (ZCW).
# Delta Electronics, Inc. 2012

### 0. Define variables
THISPATH=$(pwd)
BASEPATH=${THISPATH%%/deploy}
ZONEPATH=${BASEPATH}/ZONES

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
apache2ctl stop

### 4. Setup ZCW
cd ${THISPATH}
## 4.1 Setup Apache HTTP Server
#Execute setup-instance command
source /usr/share/doc/apache2/examples/setup-instance zcw
#Enable wsgi
a2enmod-zcw wsgi
#Copy Apache configurations
cp -r apache2-zcw/ /etc/
#Replace PROJECTNAME variable
sed -i "s/PROJECTNAME/${PROJECTNAME}/g" /etc/apache2-zcw/sites-available/default

#Copy Django project
mkdir /var/www-zcw
cp -r ${PROJECTPATH}/ /var/www-zcw/
#Collects the static files into STATIC_ROOT
python /var/www-zcw/${PROJECTNAME}/manage.py collectstatic --noinput
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

sed -i "s/exit 0/python \/usr\/local\/bin\/zcw_init\.py \nexit 0/g" /etc/rc.local

### Finish deploying ZCW
echo "Finish deploying ZCW"
exit 0
