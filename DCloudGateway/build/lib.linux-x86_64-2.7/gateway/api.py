import json
import os
import ConfigParser
import common

log = common.getLogger(name="GatewayAPI", conf="/etc/delta/Gateway.ini")

def get_storage_account():
	log.info("get_storage_account start")

	op_ok = False
	op_msg = 'Storage account read failed unexpectedly.'
	op_storage_url = ''
	op_account = ''

	try:
        	op_fh = open('/root/.s3ql/authinfo2','r')
	except IOError:
		op_msg = 'Storage account information not created or not readable.'
		log.error(op_msg)
	else:
 		op_fh.close()
 		op_config = ConfigParser.SafeConfigParser()
        try:
		op_config.read('/root/.s3ql/authinfo2')
		for section in op_config.sections():
			op_storage_url = op_config.get(section, 'storage-url')
			op_account = op_config.get(section, 'backend-login')
			op_ok = True
			op_msg = 'Obtained storage account information'
        except:
		op_msg = 'Unable to obtain storage url or login info.' 
		log.error(op_msg)
    
	return_val = {'result' : op_ok,
		         'msg' : op_msg,
                        'data' : {'storage_url' : op_storage_url, 'account' : op_account}}

	log.info("get_storage_account end")
	return json.dumps(return_val)
