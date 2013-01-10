#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for controlling HTTP proxy

import os
import json
import subprocess
from gateway import api
from gateway import common

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

def set_http_proxy(setting):
    """
    Toggle squid service to be on or off.
    Accept an "on" or "off" string as input.

    @type setting: string
    @param setting: Status of HTTP proxy. Should be 'on' or 'off'
    @rtype: string
    @return: JSON object.
             - result: Function call result. True or False
             - code: Function call return code
             - msg: Return message
    """
    op_ok = False
    op_code = "000"
    op_msg = ''
    return_val = {'result': op_ok,
                  'code': op_code,
                  'msg': op_msg}

    if setting == "on":
        cmd = "sudo service squid3 start"
    elif setting == "off":
        cmd = "sudo service squid3 stop"
    else:
        op_msg = 'Invalid argument. Must be "on" or "off"'
        return_val['msg'] = op_msg
        return json.dumps(return_val)

    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    po.wait()

    # check proxy status is really set
    proxy_status = "off"
    if api._check_process_alive('squid3'):
        proxy_status = "on"
        
    if setting == proxy_status:            
        op_ok = True
        op_code = '100'
        # wthung, 2012/12/10
        # write to savebox.ini
        sb_ini = '/etc/delta/savebox.ini'
        try:
            sb_config = api.getSaveboxConfig()
            sb_config.set('squid3', 'start_on_boot', setting)
            with open(sb_ini, 'wb') as op_fh:
                sb_config.write(op_fh)
            # save conf. To avoid UI hang, use an external process
            cmd = 'sudo python /etc/delta/save_conf.py'
            subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        except Exception as e:
            log.debug('Failed to save squid3 start_on_boot setting. Error=%s' % str(e))
    else:
        # current proxy status is not the same as api set
        op_ok = False
        op_code = '000'
        op_msg = 'Failed to turn %s http proxy.' % setting

    return_val = {'result': op_ok,
                  'code':   op_code,
                  'msg':    op_msg}

    return json.dumps(return_val)

if __name__ == '__main__':
    print set_http_proxy('on')
