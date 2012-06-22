#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for controlling HTTP proxy 

import os
import json
import api
import common
import ConfigParser
import subprocess
import BackupToCloud
from BackupToCloud import *

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

#----------------------------------------------------------------------
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
	#~ log.info("[2] get_configuration_backup_info start")
	backup_info = _get_latest_backup()
	
	#~ Case 1. There is no container "config" 
	if backup_info is None:
		op_ok = False
		op_data = {'backup_time': None}
		op_code = "000"
		op_msg = "There is no [config] container at Swift."
	else:
		dt = backup_info['datetime']
		backup_time = "%s/%s/%s %s:%s"%(dt[0:4], dt[4:6], dt[6:8], dt[8:10], dt[10:12])
		#~ print backup_time
		op_ok = True
		op_data = {'backup_time': backup_time}
		op_code = "100"
		op_msg = None

	return_val = {'result'  : op_ok,
				  'data'	: op_data,
				  'code'	: op_code,
				  'msg'	 : op_msg	}
	
	#~ log.info("[2] get_configuration_backup_info end")		  
	return json.dumps(return_val)

#----------------------------------------------------------------------
def _get_latest_backup():
    [url, login, password] = _get_Swift_credential()
    cmd = "swift -A https://%s/auth/v1.0 -U %s -K %s list config"%(url, login, password)
    po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    res = po.stdout.readlines()
    po.wait()    
        
    #~ Case 1. There is no container "config" 
    if "not found" in res:
        return None
    else:
        #~ Case 2. Get a list of files
        latest_dt = -999
        for fn in res:   # find latest backup
            dt = fn[0:12]
            if dt > latest_dt:
                latest_dt = dt
                fname = fn      # fname is the file name of latest backup
     
        backup_info = {'datetime':latest_dt, 'fname':fname}
        return backup_info

    return None
    
#----------------------------------------------------------------------
def save_gateway_configuration():
    """
    Save current configuration to Cloud.
    """
    return_val = {'result'  : False,
                  'code'    : '001',
                  'msg'     : 'config backup is fail!'}
    fileList = ['/etc/delta/network.info',
                '/etc/delta/gw_schedule.conf',
                '/etc/exports',
                '/etc/samba/smb.conf',
                ]
    swiftData = _get_Swift_credential()
    try:
        swiftObj = SwiftClient(swiftData[0], swiftData[1], swiftData[2])
        backupToCloudObj = BackupToCloud(fileList, swiftObj)
        backupToCloudObj.backup()
        return_val = {'result'  : True,
                      'code'    : '100',
                      'msg'     : 'config backup success'}
        return json.dumps(return_val)
    except BackupError as e:
        return_val = {'result'  : False,
                      'code'    : e.code,
                      'msg'     : e.msg}
        return json.dumps(return_val)

#----------------------------------------------------------------------
def restore_gateway_configuration():
	"""
	Restore latest configuration from Cloud.
	"""
	log.info("[2] restore_gateway_configuration start")
	
	tmp_dir = "/tmp/restore_config/"
	os.system("rm -r "+tmp_dir)			#~ clean old temp. data
	os.system("mkdir -p "+tmp_dir)		#~ prepare tmp working directory.
	backup_info = _get_latest_backup()	#~ read backup file info from cloud.

	if backup_info is None:
		op_ok = False
		op_code = "000"
		op_msg = "There is no [config] container at Swift."
	else:
		fname = backup_info['fname']
		[url, login, password] = _get_Swift_credential()
		cmd = "cd %s; " % (tmp_dir)
		cmd += "swift -A https://%s/auth/v1.0 -U %s -K %s download config %s"%(url, login, password, fname)
		os.system(cmd)
		# ^^^ 1. download last backup file.
		cmd = "cd %s; tar zxvf %s " % (tmp_dir, fname)
		os.system(cmd)
		# ^^^ 2. untar the backup file.
		print
		try:
			fp = open(tmp_dir+'metadata.txt')
			JsonData = fp.read();
			bak = json.loads(JsonData)
			for b in bak.items():
				v = b[1]	# get a dict of 'file'				
				# ToDo...
				# ^^^ 3.1. upgrade config files if necessary.
				cmd = "chown %s:%s %s%s" % (v['user'], v['group'], tmp_dir, v['fname'])				
				os.system(cmd)		# chage file owner
				cmd = "chmod %s %s%s" % (v['chmod'], tmp_dir, v['fname'])				
				os.system(cmd)		# chage file access
				cmd = "cd %s; mv %s %s" % (tmp_dir, v['fname'], v['fpath'])
				os.system(cmd)
				# ^^^ 3.2. put config files back to their destination folder.

			# ^^^ 3.3. restart gateway services
			api.restart_nfs_service()

			op_ok = True
			op_code = "100"
			op_msg = None
		# ^^^ 3. parse metadata. (where should config files be put to)
		except IOError as e:
			op_ok = False
			op_code = "001"
			op_msg = "Errors occurred when restoring configuration files."
		
	#~ end of if-else
	
	return_val = {'result'  : op_ok,
				  'code'	: op_code,
				  'msg'	 : op_msg	}
			  
	log.info("[2] restore_gateway_configuration stop")
	return json.dumps(return_val)



#----------------------------------------------------------------------
    
if __name__ == '__main__':
    #~ info = get_configuration_backup_info()
    #~ print info
    #~ res = restore_gateway_configuration()
    res = save_gateway_configuration()
    print res
    pass
