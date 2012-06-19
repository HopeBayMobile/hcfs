#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for controlling HTTP proxy 

import os
import json
#import api
import common
import ConfigParser
import subprocess

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

def _get_Swift_credential():
    log.info("_get_Swift_credential start")
    url = None
    login = None
    password = None
    
    try:
        config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2') as op_fh:
            config.readfp(op_fh)

        section = "CloudStorageGateway"
        url = config.get(section, 'storage-url').replace("swift://","")
        login = config.get(section, 'backend-login')
        password = config.get(section, 'backend-password')

    except Exception as e:
        log.error("Failed to _get_Swift_credential for %s"%str(e))
    finally:
        log.info("_get_Swift_credential end")
    
    return [url, login, password]


#----------------------------------------------------------------------
def get_configuration_backup_info():
    """
    Get the information of latest backup configuration.
    1. Get connection info for Swift.
    2. Probe whether there is a config file in Swift.
    3. If yes, download it and get the last backup date and time
    """
    [url, login, password] = _get_Swift_credential()
    cmd = "swift -A https://%s/auth/v1.0 -U %s -K %s list config"%(url, login, password)
    po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()    
    print output
    #~ Case 1. There is no container "config" 
    
    # do something above ...
    op_ok = True
    op_data = {'backup_time': "2012/06/31 15:31"}
    op_code = "000"
    op_msg = None

    return_val = {'result'  : op_ok,
                  'data'    : op_data,
                  'code'    : op_code,
                  'msg'     : op_msg    }
              
    return json.dumps(return_val)

#----------------------------------------------------------------------
def save_gateway_configuration():
    """
    Save current configuration to Cloud.
    """
    # do something here ...
    op_ok = True
    op_code = "000"
    op_msg = None
    
    return_val = {'result'  : op_ok,
                  'code'    : op_code,
                  'msg'     : op_msg    }
              
    return json.dumps(return_val)
    

#----------------------------------------------------------------------
def restore_gateway_configuration():
    """
    Restore latest configuration from Cloud.
    """
    # do something here ...
    op_ok = True
    op_code = "000"
    op_msg = None
    
    return_val = {'result'  : op_ok,
                  'code'    : op_code,
                  'msg'     : op_msg    }
              
    return json.dumps(return_val)
    
if __name__ == '__main__':
    get_configuration_backup_info()
    pass
