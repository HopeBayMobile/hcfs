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
    9 = DO_UPGRADE
"""

import os
import time
import simplejson as json
import common
import subprocess
import thread
#~ from gateway import api

logger = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
status_file = "/var/log/gateway_upgrade.status"
progress_file = "/var/log/gateway_upgrade.progress"
pkg_version_file = "/var/log/downloaded_package.version"

class InvalidVersionString(Exception): pass

#----------------------------------------------------------------------
def set_upgrade_status(val):
    try:
        fh = open(status_file, 'w')
        fh.write( str(val) )
        op_ok = True
    except Exception as e:
        logger.debug(str(e))
        print(str(e))
        op_ok = False

    return op_ok


def get_upgrade_status(unittest=False, test_param=None):
    """
    Get the status code of upgrade
    Return value = '{"code":status_code, "progress":download_progress}'
    Status Code:
        0 = UNKNOWN
        1 = NO_UPGRADE_AVAILABLE
        3 = NEW_UPGRADE_AVAILABLE
        5 = DO_DOWNLOAD
        7 = DOWNLOAD_DONE
        9 = DO_UPGRADE
    Unit Test / mock function usage
        get_upgrade_status(unittest=True, test_param = 
                {'code':status_code, 'progress':download_progress} )
    """
    if unittest:
        if 'code' in test_param.keys():
            try:
                code = test_param['code']
                progress = test_param['progress']
                set_upgrade_status(code)
            except Exception as e:
                code = 0
                progress = 0
                set_upgrade_status(code)
                
    else:       ## not for unit test
        try:
            #thread.start_new_thread(get_available_upgrade, ())  
            upgrade_info = get_available_upgrade() 
            ## ^^^ trigger status change if new upgrade is available
            fh = open(status_file, 'r')
            code = int( fh.read() )
            progress = get_download_progress()
            if progress == -1:
                set_upgrade_status(1)
                os.system("echo '0' > /var/log/gateway_upgrade.progress")
            
            if code == 7:   ## check if downloaded package has been expired
                fh = open(pkg_version_file, 'r')
                downloaded_ver = fh.read()
                latest_ver = json.loads(upgrade_info)['version']
                if downloaded_ver != latest_ver:
                    print "downloaded package is expired."
                    set_upgrade_status(3)
                    
        except Exception as e:
            set_upgrade_status(1)   ## in case of status file is not generated
            logger.debug(str(e))
            code = 0
            progress = 0
        
    return_val = {'code':     code,
                  'progress': progress}

    return json.dumps(return_val)



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
                set_upgrade_status(3)
            else:
                op_ok = True
                op_code = 0x5
                version = None
                description = None
                op_msg = "There is no newer update."
                set_upgrade_status(1)
    else:   ## not running unittest
        res = ''
        gateway_ver_file = '/dev/shm/gateway_ver'
        try:
            # wthung, 2012/8/8
            # move apt-show-versions to gw_bktask.py to speed up
            # the result is stored in /dev/shm/gateway_ver
            if not os.path.exists(gateway_ver_file):        
                res = ''    ## version file is not ready
            else:
                with open(gateway_ver_file, 'r') as fh:
                    res = fh.readline()

            # if the result is '', it means no available update.
            if len(res) < 5:
                op_code = 0x5
                op_ok = False
                op_msg = "There is no newer update."
                version = None
                description = ''
                set_upgrade_status(1)
            else:
                t = res.split(' ')
                ver = t[-1].replace('\n', '')
                op_code = 0x6
                op_ok = True
                op_msg = "A newer update is available."
                version = ver
                description = ''
                fh = open(status_file, 'r')
                code = int( fh.read() )
                if code == 1:
                    set_upgrade_status(3)
        except:
            op_code = 0x8022
            op_ok = False
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
            op_ok = True
            op_code = 0x16
            op_msg = "Set upgrade status to 9=DO_UPGRADE."
            # ^^^ assign return value
            logger.info("Set upgrade status to 9=DO_UPGRADE; update to %s (from %s)" % (new_ver, curr_ver))
            # ^^^ write log info
            op_res = set_upgrade_status(9)  ##   9=DO_UPGRADE
            if not op_res:
                raise Exception
        else:
            op_ok = False
            op_code = 0x15
            op_msg = "SAVEBOX is already using the latest version."
            logger.info("There is no new update detected when \
                        running upgrade_gateway()")
    except:
        op_ok = False
        op_code = 0x8022
        logger.info("Setting upgrade status to 9=DO_UPGRADE failed." % (new_ver) )

    # do something here ...

    return_val = {'result': op_ok,
                  'code':   op_code,
                  'msg':    op_msg}
    
    #~ # wthung, 2012/11/14
    #~ # in the upgrade, savebox services have been stopped.
    #~ # they cannot get return code from api.
    #~ # we must dump the result to a file for communication.
    #~ with open('/dev/shm/upgrade_result', 'w') as fh:        
        #~ json.dump(return_val, fh)

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
                op_res = set_upgrade_status(7)  ##   5 = DOWNLOAD_DONE
            else:
                op_ok = False
                op_code = 0x8022
                op_res = set_upgrade_status(5)  ##   5 = DOWNLOAD_IN_PROGRESS
    else:   ## not doing unittest
        try:
            op_res = set_upgrade_status(5)  ##   5 = DOWNLOAD_IN_PROGRESS
            if not op_res:
                raise Exception
            #~ ## download DEB files to cache
            cmd = "bash /usr/local/bin/do_download_upgrade_package.sh &"
            a = os.system(cmd)
            ## set a log file to record the version of downloaded package
            ## this is used for checking whether downloaded package is expired.
            upgrade_info = get_available_upgrade()
            ver = json.loads(upgrade_info)['version']
            fh = open(pkg_version_file, 'w')
            fh.write( ver )
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
    Return value: progress=[-1, 0..100]
    progress = -1 means donwload failed.
    progress = 0 means 0% is completed.
    progress = 90 means 90% is completed.
    Unit Test / mock function usage
        get_download_progress(unittest=True, test_param={'progress':60})
    """
    op_msg = ''
    if unittest:
        if 'progress' in test_param.keys():
            op_progress = test_param['progress']
    
    else:   ## not for unittest
        try:
            fh = open(progress_file, 'r')
            op_progress = int( fh.read() )
        except Exception as e:
            logger.debug(str(e))
            code = 0
            op_progress = 0


    return op_progress
    


if __name__ == '__main__':
    res = get_upgrade_status()
    print res
    #~ print res
    pass
