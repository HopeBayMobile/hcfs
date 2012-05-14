#import nose
import sys
import os
import json
import random
import time

# Add gateway sources
DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
BASEDIR = os.path.dirname(DIR)
sys.path.insert(0, os.path.join(BASEDIR, 'src'))

# Import packages to be tested
from gateway.gw_log_status import * 

def main_test():

	status = ""
	data = {} 
	
	print "test get_gateway_status()"
	status = get_gateway_status()
	data = json.loads(status)

	print data
	
	if data["result"] == False:
		print "Fail\n\n"
	else:
		print "Success\n\n"


	print "\n\n"
	print "test get_gateway_system_log()"
	status = get_gateway_system_log(0, 1, 1)
	data = json.loads(status)
	print status

	if data["result"] == False:
		print "Fail\n\n"
	else:
		print "Success\n\n"
	

if __name__ == "__main__":
	main_test()
	pass
