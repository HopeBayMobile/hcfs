#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for gateway remote upgrade

"""
    There will be a status file recording current status of gateway upgrade.
    File path = /var/log/gateway_upgrade.status
    Status Code:
    0 = UNKNOWN
    1 = NO_UPGRADE_AVAILABLE
    3 = NEW_UPGRADE_AVAILABLE
    5 = DO_DOWNLOAD
    7 = DOWNLOAD_DONE
    9 = DO_UPGRAD
"""

import os
import time
import simplejson as json
import common
import subprocess
#~ from gateway import api

logger = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
status_file = "/var/log/gateway_upgrade.status"

class InvalidVersionString(Exception): pass

#----------------------------------------------------------------------
def _set_upgrade_status(val):
    fh = open(status_file, 'w')
    fh.write( str(val) )
    
def _get_upgrade_status():
    fh = open(status_file, 'r')
    s = fh.read()
    return int(s)


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
        op_code = 0xB
        op_msg = "Getting software version was successful."
        version = ver
    except:
        op_ok = False
        op_code = 0x8015
        op_msg = "Getting software version failed."
        version = ''

    return_val = {'result':  op_ok,
                  'version': version,
                  'code':    op_code,
                  'msg':     op_msg}

    return json.dumps(return_val)


#----------------------------------------------------------------------
def get_available_upgrade(unittest=False, test_param=None):
    """
    Check whether there is new upgrade available
    Return value = '{"result":true/false, "code":op_code, "description":des,
                    "version":ver, "msg":op_msg}'
    op_code defintion:
        0x5:       No new update
        0x6:       Has new update
        0x8022:    Fail, error during apt-get download.
    Unit Test / mock function usage
        get_available_upgrade(unittest=True, test_param = 
                {'new_update':True, 'version':ver, 'description':des} )
    """
    if unittest:
        if 'new_update' in test_param.keys():
            if test_param['new_update']:
                op_ok = True
                op_code = 0x6
                version = test_param['version']
                description = test_param['description']
                op_msg = "A newer update is available."
            else:
                op_ok = True
                op_code = 0x5
                version = None
                description = None
                op_msg = "There is no newer update."
    else:   ## not running unittest
        res = ''
        gateway_ver_file = '/dev/shm/gateway_ver'
        try:
            # wthung, 2012/8/8
            # move apt-show-versions to gw_bktask.py to speed up
            # the result is stored in /dev/shm/gateway_ver
            # only do apt-show-versions if file is not existed
            if not os.path.exists(gateway_ver_file):        
                logger.debug("%s is not existed. Spend some time to check gateway version" % gateway_ver_file)
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
                op_code = 0x5
                op_msg = "There is no newer update."
                version = None
                description = ''
            else:
                t = res.split(' ')
                ver = t[-1].replace('\n', '')
                op_code = 0x6
                op_msg = "A newer update is available."
                version = ver
                description = ''
            # query for new updates
        except:
            op_code = 0x8022
            op_msg = "Querying new update failed."
            version = None
            description = None

    # ToDo: description field.

    return_val = {'result':      op_ok,
                  'version':     version,
                  'description': description,
                  'code':        op_code,
                  'msg':         op_msg}

    return json.dumps(return_val)


#----------------------------------------------------------------------
def upgrade_gateway():
    """
    Upgrade gateway to the latest software version.
    op_code defintion:
        0x16:    Success
        0x8022:    Fail, cannot run apt-get install.
        0x15:    Fail, there is no new update.
    """
    logger.debug("Try to upgrade gateway")
    try:
        t = get_gateway_version()
        curr_ver = json.loads(t)['version']
        t = get_available_upgrade()
        new_ver = json.loads(t)['version']
        # ^^^ read version info.
        if new_ver is not None:
            #~ download DEB files to cache
            cmd = "sudo apt-get download dcloud-gateway dcloudgatewayapi s3ql savebox"
            a = os.system(cmd)
            apt_cache_dir = "/var/cache/apt/archives"
            cmd = "mv *.deb %s" % (apt_cache_dir)
            a = os.system(cmd)            
            #~ Update "apt-get" index   #-- Ovid Wu <ovid.wu@delta.com.tw> Mon, 06 Aug 2012 06:18:03 +0000
            cmd = "dpkg-scanpackages %s > %s/Packages" % (apt_cache_dir, apt_cache_dir)
            a = os.system(cmd)
            cmd = "gzip -f %s/Packages" % (apt_cache_dir)
            a = os.system(cmd)
            cmd = "apt-get update"
            a = os.system(cmd)			
            #~ write start upgrade flag; this should be after local cache is downloaded
            cmd = "echo 'upgrading' > /root/upgrading.flag"
            a = os.system(cmd)
            #~ upgrade gateway
            cmd = "sudo apt-get -u install -y --force-yes dcloud-gateway 2> /tmp/log.txt"
            a = os.system(cmd)
            cmd = "sudo apt-get -u install -y --force-yes dcloudgatewayapi"
            os.system(cmd)
            cmd = "sudo apt-get -u install -y --force-yes s3ql"
            os.system(cmd)
            cmd = "sudo apt-get -u install -y --force-yes savebox"
            os.system(cmd)
            #~ check result
            if a == 0:
                op_ok = True
                op_code = 0x16
                op_msg = "Updating to the latest SAVEBOX version was successful."
                # ^^^ assign return value
                logger.info("Gateway is updated to %s (from %s)" % (new_ver, curr_ver))
                # ^^^ write log info
                cmd = "rm /root/upgrading.flag"     # clear upgrade flag
                a = os.system(cmd)
            else:
                op_ok = False
                op_code = 0x8022
                op_msg = "Updating to the latest SAVEBOX version failed."
                logger.info("Updating to the latest SAVEBOX version %s failed." % (new_ver) )
        else:
            op_ok = False
            op_code = 0x15
            op_msg = "SAVEBOX is already using the latest version."
            logger.info("There is no new update detected when \
                        running upgrade_gateway()")
    except:
        op_ok = False
        op_code = 0x00000403
        logger.info("Updating to the latest SAVEBOX version %s failed." % (new_ver) )

    # do something here ...

    return_val = {'result': op_ok,
                  'code':   op_code,
                  'msg':    op_msg}
    
    # wthung, 2012/11/14
    # in the upgrade, savebox services have been stopped.
    # they cannot get return code from api.
    # we must dump the result to a file for communication.
    with open('/dev/shm/upgrade_result', 'w') as fh:        
        json.dump(return_val, fh)

    return json.dumps(return_val)


#----------------------------------------------------------------------
def download_package(unittest=False, test_param=None):
    """
    Download package from APT server
    Return value = '{"result":true/false, "code"=op_code}'
    op_code defintion:
        0x16:      Success
        0x8022:    Fail, error during apt-get download.
    Unit Test / mock function usage
        download_package(unittest=True, test_param={'success':True})
        download_package(unittest=True, test_param={'success':False})
    """
    if unittest:
        if 'success' in test_param.keys():
            if test_param['success']:
                op_ok = True
                op_code = 0x16
            else:
                op_ok = False
                op_code = 0x8022
    else:   ## not doing unittest
        try:
            _set_upgrade_status(5)  ##   5 = DOWNLOAD_IN_PROGRESS
            #~ ## download DEB files to cache
            cmd = "./do_download_upgrade_package.sh &"
            a = os.system(cmd)
            ## assign return values
            op_ok = True
            op_code = 0x16            
        except Exception as e:
            logger.debug(str(e))
            print(str(e))
            op_ok = False
            op_code = 0x8022

    return_val = {'result': op_ok,
                  'code':   op_code}
    return json.dumps(return_val)

#----------------------------------------------------------------------
def get_download_progress(unittest=False, test_param=None):
    """
    Get the progress of downloading packages
    Return value = '{"result":true/false, "code"=op_code, "progress"=[0..100]}'
    progress = 0 means 0% is completed.
    progress = 90 means 90% is completed.
    op_code defintion:
        0x16:      Success
        0x8022:    Fail, error in getting download progress
    Unit Test / mock function usage
        get_download_progress(unittest=True, test_param={'progress':60})
    """
    op_msg = ''
    if unittest:
        if 'progress' in test_param.keys():
            op_ok = True
            op_code = 0x16
            op_progress = test_param['progress']
    
    return_val = {'result': op_ok,
                  'code':   op_code,
                  'progress':    op_progress}

    return json.dumps(return_val)
    

#----------------------------------------------------------------------
def download_complete(unittest=False, test_param=None):
    """
    Check whether download is completed.
    Return value = '{"result":true/false, "code"=op_code}'
    op_code defintion:
        0x16:      Success
        0x8022:    Fail, error in getting download progress
    Unit Test / mock function usage
        download_complete(unittest=True, test_param={'success':True})
        download_complete(unittest=True, test_param={'success':False})
    """
    if unittest:
        if 'success' in test_param.keys():
            if test_param['success']:
                op_ok = True
                op_code = 0x16
            else:
                op_ok = False
                op_code = 0x8022
    else:
        try:
            val = int( _get_upgrade_status() )
            if val == 7:
                op_ok = True
                op_code = 0x16
            else:
                op_ok = False
                op_code = 0x8022
                
        except Exception as e:
            logger.debug(str(e))
            print(str(e))
            op_ok = False
            op_code = 0x8022
        
        
    return_val = {'result': op_ok,
                  'code':   op_code}
                  
    return json.dumps(return_val)



if __name__ == '__main__':
    #~ res = upgrade_gateway()
    #~ params = {'new_update':True, 'version':'1.1.9.9999', 'description':"Test version"}
    #~ res = get_available_upgrade(unittest=True, test_param = params)
    #~ print res
    #~ 
    #~ params = {'success':True}
    #~ res = download_complete(unittest=True, test_param = params)
    #~ print res
    #~ 
    #~ params = {'success':True}    
    #~ res = download_package(unittest=True, test_param = params)
    #~ print res
    #~ 
    #~ params = {'progress':60}
    #~ res = get_download_progress(unittest=True, test_param = params)
    #~ print res

    _set_upgrade_status(5)
    val = _get_upgrade_status()
    print("upgrade status = %d" % val )

    res = download_package()
    print res
    pass
