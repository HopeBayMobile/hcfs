#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: API function for controlling HTTP proxy 


import json
import os

def set_http_proxy( setting ):
	"""
	Toggle squid service to be on or off. Accept an "on" or "off" string as input.
	@type setting: string
	"""
	if setting=="on":
		# do something here ...
		op_ok = True
		op_code = "000"
		op_msg = None

	if setting=="off":
		# do something here ...
		op_ok = True
		op_code = "000"
		op_msg = None
	
	return_val = {'result'	: op_ok,
        	      'code'	: op_code,
        	      'msg'		: op_msg 	}
		      
	return json.dumps(return_val)
