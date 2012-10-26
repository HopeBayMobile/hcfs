#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for gateway remote upgrade

import os
import time
import simplejson as json
import common
import subprocess
from gateway import api

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

class InvalidVersionString(Exception): pass

#----------------------------------------------------------------------
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


#--------------------------------------------------
def get_gateway_version():
    """
    Get current software version of the gateway.
    """
    try:
        # read current version of gateway
        cmd = "apt-show-versions dcloud-gateway"
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,\
                                stderr=subprocess.STDOUT)
        res = po.stdout.readline()
        po.wait()
        if "upgradeable" in res:	# "dcloud-gateway/precise upgradeable from 1.0.10.20120830 to 1.0.10.20120831"
            idx = -3
        elif "uptodate" in res:	#~  "dcloud-gateway/unknown uptodate 1.0.10.20120828"
            idx = -1
        else:
            raise InvalidVersionString()

        t = res.split(' ')
        ver = t[idx].replace('\n', '')

        op_ok = True
        op_code = 0x00000092
        op_msg = "Getting software version was successful."
        version = ver
    except:
        op_ok = False
        op_code = 0x00000091
        op_msg = "Getting software version failed."
        version = ''

    return_val = {'result':  op_ok,
                  'version': version,
                  'code':    op_code,
                  'msg':     op_msg}

    return json.dumps(return_val)


#----------------------------------------------------------------------
def get_available_upgrade():
    """
    Get the info. of latest upgrade/update.
    op_code defintion:
        0x00000043:    Success and there is an update.
        0x00000041:    Success and there is NO new update.
        0x00000042:    Fail.
    """

    res = ''
    gateway_ver_file = '/dev/shm/gateway_ver'
    #~ FIXME: change the file name accordingly at gw_bktask.py.
    try:
        #~ os.system("sudo apt-get update &")     # update package info.
        #~ the apt-get update action will be ran at background process
        
        # wthung, 2012/8/8
        # move apt-show-versions to gw_bktask.py to speed up
        # the result is stored in /dev/shm/gateway_ver
        # only do apt-show-versions if file is not existed
        if not os.path.exists(gateway_ver_file):        
            log.debug("%s is not existed. Spend some time to check gateway version" % gateway_ver_file)
            cmd = "apt-show-versions -u dcloud-gateway"
            # ToDo: change to "DeltaGateway" package.
            po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, \
                                    stderr=subprocess.STDOUT)
            res = po.stdout.readline()
            po.wait()
        else:
            with open(gateway_ver_file, 'r') as fh:
                res = fh.readline()

        # if the result is '', it means no available update.
        if len(res) == 0:
            op_code = 0x00000041
            op_msg = "There is no newer update."
            version = None
            description = ''
        else:
            t = res.split(' ')
            ver = t[-1].replace('\n', '')
            op_code = 0x00000043
            op_msg = "A newer update is available."
            version = ver
            description = ''
        # query for new updates
    except:
        op_code = 0x00000042
        op_msg = "Querying new update failed."
        version = None
        description = None

    # ToDo: description field.

    return_val = {'version':     version,
                  'description': description,
                  'code':        op_code,
                  'msg':         op_msg}

    return json.dumps(return_val)


#----------------------------------------------------------------------
def upgrade_gateway(enableReboot = True):
    """
    Upgrade gateway to the latest software version.
    op_code defintion:
        0x00000402:    Success
        0x00000403:    Fail, cannot run apt-get install.
        0x00000401:    Fail, there is no new update.
    """
    log.debug("Try to upgrade gateway")
    try:
        t = get_gateway_version()
        curr_ver = json.loads(t)['version']
        t = get_available_upgrade()
        new_ver = json.loads(t)['version']
        # ^^^ read version info.
        if new_ver is not None:
            cmd = "sudo apt-get -u install -y --force-yes dcloud-gateway 2> /tmp/log.txt"
            a = os.system(cmd)
            cmd = "sudo apt-get -u install -y --force-yes dcloudgatewayapi"
            os.system(cmd)
            cmd = "sudo apt-get -u install -y --force-yes s3ql"
            os.system(cmd)
            cmd = "sudo apt-get -u install -y --force-yes savebox"
            os.system(cmd)
            # ^^^ upgrade gateway
            if a == 0:
                op_ok = True
                op_code = 0x00000402
                op_msg = "Updating to the latest gateway version was successful."
                # ^^^ assign return value
                log.info("Gateway is updated to %s (from %s)" % (new_ver, curr_ver))
                # ^^^ write log info
                if enableReboot == True:
                    #~ api.reset_gateway()
                    os.system("sudo sync;  sudo shutdown -r now")
                    # ^^^ send a reboot command to os.
                    os.system("sudo service apache2 stop")
                    # ^^^ stop apache2 service 
            else:
                op_ok = False
                op_code = 0x00000403
                op_msg = "Updating to the latest gateway version failed."
        else:
            op_ok = False
            op_code = 0x00000401
            op_msg = "Gateway is already using the latest version."
            log.info("There is no new update detected when \
                        running upgrade_gateway()")
    except:
        op_ok = False
        op_code = 0x00000403
        op_msg = "Updating to the latest gateway version failed."

    # do something here ...

    return_val = {'result': op_ok,
                  'code':   op_code,
                  'msg':    op_msg}

    return json.dumps(return_val)


if __name__ == '__main__':
    #~ res = upgrade_gateway()
    res = get_available_upgrade()
    print res
    #res = upgrade_gateway()
    #print res
    pass
