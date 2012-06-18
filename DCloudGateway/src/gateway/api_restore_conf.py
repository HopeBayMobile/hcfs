#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for controlling HTTP proxy 

import os

def get_configuration_backup_info():
	"""
	Get the information of latest backup configuration.
	"""
	
	# do something here ...
	op_ok = True
	op_data = "2012/06/31 15:31"
	op_code = "000"
	op_msg = None

	return_val = {'result'	: op_ok,
				  'data'	: op_data,
        	      'code'	: op_code,
        	      'msg'		: op_msg 	}
		      
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
	
	return_val = {'result'	: op_ok,
        	      'code'	: op_code,
        	      'msg'		: op_msg 	}
		      
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
	
	return_val = {'result'	: op_ok,
        	      'code'	: op_code,
        	      'msg'		: op_msg 	}
		      
	return json.dumps(return_val)
	
