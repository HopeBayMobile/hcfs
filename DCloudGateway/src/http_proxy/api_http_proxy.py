#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for controlling HTTP proxy

import os
import json
import subprocess


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
