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
	backup_info = _get_latest_backup()
	
	#~ Case 1. There is no container "config" 
	if backup_info is None:
		op_ok = False
		op_data = {'backup_time': None}
		op_code = "100"
		op_msg = "There is no [config] container at Swift."
	else:
		dt = backup_info['datetime']
		backup_time = "%s/%s/%s %s:%s"%(dt[0:4], dt[4:6], dt[6:8], dt[8:10], dt[10:12])
		#~ print backup_time
		op_ok = True
		op_data = {'backup_time': backup_time}
		op_code = "000"
		op_msg = None

	return_val = {'result'  : op_ok,
				  'data'	: op_data,
				  'code'	: op_code,
				  'msg'	 : op_msg	}
			  
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
				fname = fn	  # fname is the file name of latest backup
	 
		backup_info = {'datetime':latest_dt, 'fname':fname}
		return backup_info

	return None
	
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
				  'code'	: op_code,
				  'msg'	 : op_msg	}
			  
	return json.dumps(return_val)
	

#----------------------------------------------------------------------
def restore_gateway_configuration():
	"""
	Restore latest configuration from Cloud.
	"""
	tmp_dir = "/tmp/"
	backup_info = _get_latest_backup()

	if backup_info is None:
		op_ok = False
		op_code = "100"
		op_msg = "There is no [config] container at Swift."
	else:
		fname = backup_info['fname']
		[url, login, password] = _get_Swift_credential()
		cmd = "cd %s; " % (tmp_dir)
		cmd += "swift -A https://%s/auth/v1.0 -U %s -K %s download config %s"%(url, login, password, fname)
		print cmd
		os.system(cmd)
		# ^^^ download last backup file.
		cmd = "cd %s; tar zxvf " % (tmp_dir, fname)
		
		
	# do something here ...
	op_ok = True
	op_code = "000"
	op_msg = None
	
	return_val = {'result'  : op_ok,
				  'code'	: op_code,
				  'msg'	 : op_msg	}
			  
	return json.dumps(return_val)

#----------------------------------------------------------------------
	
if __name__ == '__main__':
	#~ info = get_configuration_backup_info()
	#~ print info
	res = restore_gateway_configuration()
	print res
	pass
