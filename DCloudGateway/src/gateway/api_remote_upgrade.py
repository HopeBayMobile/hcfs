#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for gateway remote upgrade

import os
import json
import api
import common
import ConfigParser
import subprocess

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

#----------------------------------------------------------------------
def _read_command_log(log_fname):
	"""
	Read back an operation log.
	"""
	try:
		fileIN = open(log_fname, "r")
		fc = fileIN.read()
		fc = fc.replace("\n"," & ")
		fc = fc.replace("'","")   # strip ' char
	except:
		fc = ''

	return fc
	
#----------------------------------------------------------------------	
def get_gateway_version():
	"""
	Get current software version of the gateway.
	"""
	try:
		cmd = "apt-show-versions s3ql"	# ToDo: change to "DeltaGateway" package.
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		res = po.stdout.readline();		po.wait()	
		t = res.split(' ');		ver = t[-1].replace('\n','')
		# read current version of gateway
		op_ok = True;	op_code = "100";	op_msg = None;	version = ver
	except:
		op_ok = False
		op_code = "000"
		op_msg = "Cannot get software version."
		version = ''
		
	return_val = {'result'	: op_ok,
				  'version'	: version,
        	      'code'	: op_code,
        	      'msg'		: op_msg 	}
		      
	return json.dumps(return_val)

#----------------------------------------------------------------------
def get_available_upgrade():
	"""
	Get the info. of latest upgrade/update.
	op_code defintion:
		100:	Success and there is an update.
		110:	Success and there is NO new update.
		000:	Fail.
	"""
	try:
		cmd = "apt-show-versions -u s3ql"	# ToDo: change to "DeltaGateway" package.
		po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
		res = po.stdout.readline();		po.wait()	
		if len(res)==0:		# if the result is '', it means no available update.
			op_code = "110";	op_msg = "There is no update.";		
			version = None;		description = ''
		else:
			t = res.split(' ');		ver = t[-1].replace('\n','')
			op_code = "100";	op_msg = None;	
			version = ver;		description = ''
		# query for new updates
	except:
		op_code = "000";	op_msg = "Failed to query new updates.";	
		version = None;		description = None
	
	# ToDo: description field.
	
	return_val = {'version'	: version,
				  'description' : description,
        	      'code'	: op_code,
        	      'msg'		: op_msg 	}
		      
	return json.dumps(return_val)
	

#----------------------------------------------------------------------
def upgrade_gateway():
	"""
	Upgrade gateway to the latest software version.
	op_code defintion:
		100:	Success
		001:	Fail, cannot run apt-get install.
		002:	Fail, apt-get install occurs error.
		003:	Fail, there is no new update.
	"""
	try:
		t = get_gateway_version()
		curr_ver = json.loads(t)['version']
		t = get_available_upgrade()
		new_ver = json.loads(t)['version']
		# ^^^ read version info.
		if new_ver is not None:
			cmd = "apt-get install -y s3ql 2> /tmp/log.txt"		# ToDo: change to "DeltaGateway" package.
			a = os.system(cmd)
			# ^^^ upgrade gateway
			if a==0:
				op_ok = True;	op_code = "100";	op_msg = None;
				# write log info
				log.info("[2] Gateway is updated to %s (from %s)" % (new_ver, curr_ver))
				# send a reboot command to os.

			else:
				op_ok = False;	op_code = "002";	
				op_msg = _read_command_log("/tmp/log.txt");
		else:
			op_ok = False;	op_code = "003";	
			op_msg = "There is no new update.";
			log.info("[1] There is no new update detected when running upgrade_gateway()")
			
	except:
		op_ok = False
		op_code = "001"
		op_msg = "Failed to upgrade software."
	
	# do something here ...
	
	return_val = {'result'	: op_ok,
        	      'code'	: op_code,
        	      'msg'		: op_msg 	}
	
	return json.dumps(return_val)
	
	
	
if __name__ == '__main__':
	res = upgrade_gateway()
	print res
	pass
