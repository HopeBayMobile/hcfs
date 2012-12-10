import os
import os.path
import time
from gateway import api
from gateway import common

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

if __name__ == '__main__':
    try:
        sb_config = api.getSaveboxConfig()
        setting = sb_config.get('squid3', 'start_on_boot')
        if setting == 'on':
            os.system('service squid3 start')
        else:
            os.system('service squid3 stop')
        
        log.debug('Squid3 status in bootup is %s' % setting)
    except Exception as e:
        log.error('Error occurred when setting squid3 status at bootup')
        log.error('%s' % str(e))
