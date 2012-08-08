#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for controlling HTTP proxy

import os
import json


def _read_command_log(log_fname):
    """
    Read back an operation log.
    """
    try:
        fileIN = open(log_fname, "r")
        fc = fileIN.read()
        fc = fc.replace("\n", " & ")
        fc = fc.replace("'", "")   # strip ' char
    except:
        fc = ''

    return fc


# ---------------------------------------------------------------------
def set_http_proxy(setting):
    """
    Toggle squid service to be on or off.
    Accept an "on" or "off" string as input.
    @type setting: string
    """
    log_fname = "/tmp/op_log.txt"

    if setting == "on":
        cmd = "sudo service squid3 restart > " + log_fname
        a = os.system(cmd)
        op_log = _read_command_log(log_fname)
        # set result values
        if a == 0 and 'fail' not in op_log:
            op_ok = True
            op_code = "100"
            op_msg = None
        else:
            op_ok = False
            op_code = "000"
            op_msg = "failed to turn on http proxy."

    if setting == "off":
        cmd = "sudo service squid3 stop > " + log_fname
        a = os.system(cmd)
        op_log = _read_command_log(log_fname)
        # set result values
        if a == 0 and 'fail' not in op_log:
            op_ok = True
            op_code = "100"
            op_msg = None
        else:
            op_ok = False
            op_code = "000"
            op_msg = "failed to turn off http proxy."

    return_val = {'result': op_ok,
                  'code':   op_code,
                  'msg':    op_msg}

    return json.dumps(return_val)
