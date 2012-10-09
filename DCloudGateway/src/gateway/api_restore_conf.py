#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.
# Function: API function for controlling HTTP proxy

import os
import json
import api
import common
import ConfigParser
import subprocess
from BackupToCloud import *

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")


def _get_Swift_credential():
    """
    get swift credential setting
    """
    log.debug("_get_Swift_credential start")
    url = None
    login = None
    password = None
    try:
        config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2') as op_fh:
            config.readfp(op_fh)

        section = "CloudStorageGateway"
        url = config.get(section, 'storage-url').replace("swift://", "")
        login = config.get(section, 'backend-login')
        password = config.get(section, 'backend-password')

    except Exception as e:
        log.error("Failed to _get_Swift_credential for %s" % str(e))
    finally:
        log.debug("_get_Swift_credential end")
    return [url, login, password]


def get_configuration_backup_info():
    """
    Get the information of latest backup configuration.
        1. Get connection info for Swift.
        2. Probe whether there is a config file in Swift.
        3. If yes, download it and get the last backup date and time
    """
    backup_info = _get_latest_backup()
    #~ Case 1. There is no container "config"
    if backup_info is None:
        op_ok = False
        op_data = {'backup_time': None}
        op_code = "000"
        op_msg = "There is no [config] container at Swift."
    else:
        backup_time = backup_info['datetime']
        #~ print backup_time
        op_ok = True
        op_data = {'backup_time': backup_time}
        op_code = "100"
        op_msg = None

    return_val = {'result': op_ok,
                  'data':   op_data,
                  'code':   op_code,
                  'msg':    op_msg}

    return json.dumps(return_val)


#----------------------------------------------------------------------
def _get_latest_backup():
    """
    get the latest backup config from swift
    """
    [url, login, password] = _get_Swift_credential()

    # wthung, 2012/9/3
    # get uesrname to use private container
    _, username = login.split(':')

    cmd = "swift -A https://%s/auth/v1.0 -U %s -K %s list %s_gateway_config" %\
          (url, login, password, username)
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, \
                          stderr=subprocess.STDOUT)
    res = po.stdout.readlines()
    po.wait()

    # wthung, 2012/9/12
    # check length of res
    # case 1. container is exist, but no content
    if len(res) == 0:
        return None
    #~ Case 2. There is no private container
    elif "not found" in res[0]:
        return None
    else:
        #~ Case 3. Get a list of files
        latest_dt = -999
        for fn in res:   # find latest backup
            if "gw_conf_backup" not in fn:
                continue

            dt = fn[0:10]
            if dt > latest_dt:
                latest_dt = dt
                fname = fn      # fname is the file name of latest backup

        if latest_dt == -999:
            return None

        backup_info = {'datetime': latest_dt, 'fname': fname}
        return backup_info

    return None


#----------------------------------------------------------------------
def _delete_oldest_backup(n):    ## n is the number of copies for retain
    """
    This function will delete an oldest backup.
    n = the number of copies for retain
    """
    [url, login, password] = _get_Swift_credential()

    # wthung, 2012/9/3
    # get uesrname to use private container
    _, username = login.split(':')

    cmd = "swift -A https://%s/auth/v1.0 -U %s -K %s list %s_gateway_config" %\
          (url, login, password, username)
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, \
                          stderr=subprocess.STDOUT)
    res = po.stdout.readlines()
    po.wait()

    # wthung, 2012/9/12
    # check length of res
    # case 1. container is exist, but no content
    if len(res) == 0:
        return None
    #~ Case 2. There is no private container
    elif "not found" in res[0]:
        return None
    else:
        #~ Case 3. Get a list of files
        oldest_dt = 'ZZZZZZZZZZZZZZZZZZZZZZZ'
        c = 0
        for fn in res:   # find latest backup
            c += 1
            if "gw_conf_backup" not in fn:
                continue

            dt = fn[0:10]
            if dt < oldest_dt:
                oldest_dt = dt
                fname = fn      # fname is the file name of latest backup

        if c > n:   # there are many copies of backup
            cmd = "swift -A https://%s/auth/v1.0 -U %s -K %s delete %s_gateway_config %s"\
                    % (url, login, password, username, fname)
            os.system(cmd)
            return None

    return None

#----------------------------------------------------------------------
def save_gateway_configuration():
    """
    Save current configuration to Cloud.
    """
    return_val = {'result': False,
                  'code':   '001',
                  'msg':    'config backup is fail!'}
    # yen, 2012/10/09. Remove outdated config files
    fileList = ['/etc/delta/gw_schedule.conf']
    
    swiftData = _get_Swift_credential()
    if swiftData[0] is None:
        return_val['code'] = '000'
        return_val['msg'] = 'There is no [config] container at Swift.'
        return json.dumps(return_val)
    try:
        swiftObj = SwiftClient(swiftData[0], swiftData[1], swiftData[2])
        
        # wthung, 2012/9/3
        # swiftData[1] is the login pair
        _, container_name = swiftData[1].split(':')

        backupToCloudObj = BackupToCloud(fileList, swiftObj)
        backuptime = backupToCloudObj.backup(container_name + "_gateway_config")
        
        # yen, 2012/10/09.
        # Delete oldest config file
        _delete_oldest_backup(5)    ## 5 is the number of copies for retain
        
        return_val = {'result'  : True,
                      'code'    : '100',
                      'data'    : {'backup_time': backuptime},
                      'msg'     : 'config backup success'}
        return json.dumps(return_val)
    except BackupError as e:
        return_val = {'result'  : False,
                      'code'    : e.code,
                      'data'    : None,
                      'msg'     : e.msg}
        return json.dumps(return_val)
    except Exception as e:
        log.error('unexception error: %s' % e)


#----------------------------------------------------------------------
def restore_gateway_configuration():
    """
    Restore latest configuration from Cloud.
    """
    log.debug("restore_gateway_configuration start")

    tmp_dir = "/tmp/restore_config/"
    os.system("rm -r " + tmp_dir)          # clean old temp. data
    os.system("mkdir -p " + tmp_dir)       # prepare tmp working directory.
    backup_info = _get_latest_backup()     # read backup file info from cloud.

    if backup_info is None:
        op_ok = False
        op_code = "000"
        op_msg = "There is no [config] container at Swift."
    else:
        fname = backup_info['fname']
        [url, login, password] = _get_Swift_credential()

        # wthung, 2012/9/3
        # get uesrname to use private container
        _, username = login.split(':')

        cmd = "cd %s; " % (tmp_dir)
        cmd += "swift -A https://%s/auth/v1.0 -U %s -K %s download %s_gateway_config %s"\
                % (url, login, password, username, fname)
        os.system(cmd)
        # ^^^ 1. download last backup file.
        cmd = "cd %s; tar zxvf %s " % (tmp_dir, fname)
        os.system(cmd)
        # ^^^ 2. untar the backup file.
        try:
            fp = open(tmp_dir + 'metadata.txt')
            JsonData = fp.read()
            bak = json.loads(JsonData)
            for b in bak.items():
                v = b[1]    # get a dict of 'file'
                # ToDo...
                # ^^^ 3.1. upgrade config files if necessary.
                cmd = "chown %s:%s %s%s" % (v['user'], v['group'], \
                                            tmp_dir, v['fname'])
                os.system(cmd)        # chage file owner
                cmd = "chmod %s %s%s" % (v['chmod'], tmp_dir, v['fname'])
                os.system(cmd)        # chage file access
                cmd = "cd %s; mv %s %s" % (tmp_dir, v['fname'], v['fpath'])
                os.system(cmd)
                # ^^^ 3.2. put config files back to their destination folder.

            # ^^^ 3.3. restart gateway services
            api.restart_nfs_service()
            api.restart_smb_service()

            op_ok = True
            op_code = "100"
            op_msg = None
        # ^^^ 3. parse metadata. (where should config files be put to)
        except:
            op_ok = False
            op_code = "001"
            op_msg = "Errors occurred when restoring configuration files."

    #~ end of if-else

    return_val = {'result': op_ok,
                  'code':   op_code,
                  'msg':    op_msg}

    log.debug("restore_gateway_configuration stop")
    return json.dumps(return_val)


#----------------------------------------------------------------------
if __name__ == '__main__':
    #res = save_gateway_configuration()
    #print res
    #info = get_configuration_backup_info()
    #print info
    res = restore_gateway_configuration()
    print res
    pass
