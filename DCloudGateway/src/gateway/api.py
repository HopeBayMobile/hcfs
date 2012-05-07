import json
import os
import ConfigParser
import common

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

def get_storage_account():
	log.info("get_storage_account start")

	op_ok = False
	op_msg = 'Storage account read failed unexpectedly.'
	op_storage_url = ''
	op_account = ''

	try:
		op_config = ConfigParser.ConfigParser()
        	with open('/root/.s3ql/authinfo2') as op_fh:
			op_config.readfp(op_fh)

		for section in op_config.sections():
			op_storage_url = op_config.get(section, 'storage-url')
			op_account = op_config.get(section, 'backend-login')
			op_ok = True
			op_msg = 'Obtained storage account information'
			break
	except IOError as e:
		op_msg = 'Storage account information not created or not readable.'
		log.error(str(e))
	except Exception as e:
		op_msg = 'Unable to obtain storage url or login info.' 
		log.error(str(e))

	return_val = {'result' : op_ok,
		         'msg' : op_msg,
                        'data' : {'storage_url' : op_storage_url, 'account' : op_account}}

	log.info("get_storage_account end")
	return json.dumps(return_val)

if __name__ == '__main__':
	#Example of log usage
	log.debug("...")
	log.warn("...")
	log.info("...")
	log.error("...")
	pass	
