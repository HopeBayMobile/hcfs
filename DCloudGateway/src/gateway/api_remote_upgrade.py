#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for controlling HTTP proxy 

import os

def get_gateway_version():
	"""
	Get current software version of the gateway.
	"""
	
	# do something here ...
	op_ok = True
	op_code = "000"
	op_msg = None
	version = "tb1-std-1.0.0~build0018"

	return_val = {'result'	: op_ok,
				  'version'	: version,
        	      'code'	: op_code,
        	      'msg'		: op_msg 	}
		      
	return json.dumps(return_val)

#----------------------------------------------------------------------
def get_available_upgrade():
	"""
	Get the info. of latest upgrade/update.
	"""
	# do something here ...
	op_code = "000"
	op_msg = None
	version = "tb1-std-1.2.0~build0035"
	description = "New features are: (a) faster R/W speed and (b) ACL support."
	
	return_val = {'version'	: version,
				  'description' : description,
        	      'code'	: op_code,
        	      'msg'		: op_msg 	}
		      
	return json.dumps(return_val)
	

#----------------------------------------------------------------------
def upgrade_gateway():
	"""
	Upgrade gateway to the latest software version.
	"""
	# do something here ...
	op_ok = True
	op_code = "000"
	op_msg = None
	
	return_val = {'result'	: op_ok,
        	      'code'	: op_code,
        	      'msg'		: op_msg 	}
	
	# send a reboot command to os.
	
	return json.dumps(return_val)
	
