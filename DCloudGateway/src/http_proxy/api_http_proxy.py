#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for controlling HTTP proxy

import os
import json
import subprocess
from gateway import api
from gateway import common

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

# wthung, 2012/12/11
def check_http_proxy_alive():
    """
    Query squid3 service status.
    Will try to parse status from savebox.ini.
    If that file can't be accessed, try to get status from process list
    
    @rtype: boolean
    @return: True if squid3 service is running. Otherwise, False.
    """
    squid3_status = False
    
    try:
        sb_config = api.getSaveboxConfig()
        setting = sb_config.get('squid3', 'start_on_boot')
        if setting == 'on':
            squid3_status = True
        else:
            squid3_status = False
    except:
        # try to get status from process list
        squid3_status = api._check_process_alive('squid3')
    
    return squid3_status

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
        cmd = "sudo service squid3 restart"
    elif setting == "off":
        cmd = "sudo service squid3 stop"
    else:
        op_msg = 'Invalid argument. Must be "on" or "off"'
        return json.dumps(return_val)

    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()

    if po.returncode == 0:
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
            log.error('Failed to save squid3 start_on_boot setting. Error=%s' % str(e))
    elif po.returncode == 1:
        if setting == "off":
            # maybe squid3 has been stopped
            op_ok = True
            op_code = '100'
            op_msg = 'http proxy has already been stopped.'
        else:
            op_ok = False
            op_code = '000'
            op_msg = 'Failed to turn %s http proxy.' % setting

    return_val = {'result': op_ok,
                  'code':   op_code,
                  'msg':    op_msg}

    return json.dumps(return_val)

if __name__ == '__main__':
    print set_http_proxy('on')
