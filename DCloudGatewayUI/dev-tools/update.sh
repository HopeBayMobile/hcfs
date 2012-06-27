#!/bin/bash

#Define variables
THISPATH=$(pwd)
BASEPATH=${THISPATH%%/DCloudGatewayUI/dev-tools}

#Stop services
service apache2 stop
/etc/init.d/celeryd stop

#backup original files
cd /var/www/
tar czvf "$(date +'%Y%m%d-%H%M%S')-backup.tar.gz" DCloudGatewayUI/

#Update the latest files
cp -r ${BASEPATH}/DCloudGatewayUI/ /var/www/

#Start services
service apache2 start
/etc/init.d/celeryd start