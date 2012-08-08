import nose
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
from gateway import common
from gateway.common import TimeoutError
from gateway.common import timeout
from gateway.proto_AccCtrl import *

def main_test():

	status = ""
	data = {} 
	
	print "test get_smb_user_list()"
	status = get_smb_user_list()
	data = json.loads(status)

	print data
	
	if data["result"] == False:
		print "Fail\n\n"


	print "\n\n"
	print "test set_smb_user_list()"
	#status = set_smb_user_list("admin", "admin")
	status = set_smb_user_list("jashing", "a")
	data = json.loads(status)
	print status

	if data["result"] == False:
		print "Fail\n\n"
	
	print "\n\n"
	print "test get_nfs_access_ip_list()"
	status = get_nfs_access_ip_list()
	data = json.loads(status)
	print data
	
	if data["result"] == False:
		print "Fail\n\n"

	
	print "\n\n"
	print "test get_nfs_access_ip_list()"
	arr = ["127.0.0.1", "127.0.0.2"]
	status = set_nfs_access_ip_list(arr)
	data = json.loads(status)
	print data

	if data["result"] == False:
		print "Fail\n\n"
	


if __name__ == "__main__":
	main_test()
	pass
