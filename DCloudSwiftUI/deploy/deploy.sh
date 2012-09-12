#! /bin/bash
# This script is used to deploy Zone Config Wizard (ZCW).
# Delta Electronics, Inc. 2012

### 0. Define variables
THISPATH=$(pwd)
BASEPATH=${THISPATH%%/deploy}
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

### 1. Configure ZCW
#Turn off Apache Daemon
apache2ctl-zcw stop
update-rc.d -f apache2 remove

### 2. Setup ZCW
cd ${THISPATH}

if [ -e /etc/apache2-$SUFFIX ] ; then
	echo "/etc/apache2-$SUFFIX already exists"
	echo "remove /etc/apache2-$SUFFIX"
	rm -rf /etc/apache2-$SUFFIX

        echo "remove /usr/local/sbin/*-$SUFFIX"
        rm -rf /usr/local/sbin/*-$SUFFIX

        while [ -e /var/log/apache2-$SUFFIX ]; do
           sleep 2
           echo "remove /var/log/apache2-$SUFFIX"
           rm -rf /var/log/apache2-$SUFFIX
        done        

        echo "remove /var/www-$SUFFIX"
        rm -rf /var/www-$SUFFIX
fi

## 3. Syncdb and create a superuser with username/password=admin/admin
if [ -e ../${PROJECTNAME}/sqlite3.db ]; then
	rm -rf ../${PROJECTNAME}/sqlite3.db
fi

echo ${PROJECTPATH}
python <<EOF
import pexpect
child = pexpect.spawn('python ${PROJECTPATH}/manage.py syncdb')
child.expect('Would you like to create one now')
child.sendline('yes')
child.expect("Username \(leave blank to use 'root'\):")
child.sendline('admin')
child.expect("E-mail address:")
child.sendline("ctbd@delta.com.tw")
child.expect('Password:')
child.sendline('admin')
child.expect('Password \(again\)')
child.sendline('admin')
child.expect(pexpect.EOF)
EOF

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
mkdir -p /var/www-zcw

#Add a directory for public network interface
mkdir -p /var/www-zcw/cfg

cp -r ${PROJECTPATH}/ /var/www-zcw/
#Collects the static files into STATIC_ROOT
python /var/www-zcw/${PROJECTNAME}/manage.py collectstatic --noinput

#FIXME
#cp -r /usr/local/lib/python2.7/dist-packages/delta_wizard-0.1-py2.7.egg/delta/wizard/static/wizard/ /var/www-${SUFFIX}/${PROJECTNAME}/${PROJECTNAME}/static

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

apache2ctl-zcw start

### Finish deploying ZCW
echo "Finish deploying ZCW"
exit 0
