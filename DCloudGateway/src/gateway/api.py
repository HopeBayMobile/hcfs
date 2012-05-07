import json
import os
import ConfigParser
import common
import subprocess
import time

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")

class TestStorageError(Exception):
	pass

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

		section = "CloudStorageGateway"
		op_storage_url = op_config.get(section, 'storage-url')
		op_account = op_config.get(section, 'backend-login')
		op_ok = True
		op_msg = 'Obtained storage account information'

	except IOError as e:
		op_msg = 'Unable to access /root/.s3ql/authinfo2.'
		log.error(str(e))
	except Exception as e:
		op_msg = 'Unable to obtain storage url or login info.' 
		log.error(str(e))

	return_val = {'result' : op_ok,
		         'msg' : op_msg,
                        'data' : {'storage_url' : op_storage_url, 'account' : op_account}}

	log.info("get_storage_account end")
	return json.dumps(return_val)

def apply_storage_account(storage_url, account, password, test=True):
	log.info("apply_storage_account start")

	op_ok = False
	op_msg = 'Failed to apply storage accounts for unexpetced errors.'

	try:
		op_config = ConfigParser.ConfigParser()
		#Create authinfo2 if it doesn't exist
		if not os.path.exists('/root/.s3ql/authinfo2'):
			os.system("mkdir -p /root/.s3ql")
			os.system("touch /root/.s3ql/authinfo2")
			os.system("chmod 600 /root/.s3ql/authinfo2")

        	with open('/root/.s3ql/authinfo2','rb') as op_fh:
			op_config.readfp(op_fh)

		section = "CloudStorageGateway"
		if not op_config.has_section(section):
			op_config.add_section(section)

		op_config.set(section, 'storage-url', storage_url)
		op_config.set(section, 'backend-login', account)
		op_config.set(section, 'backend-passphrase', password)

		with open('/root/.s3ql/authinfo2','wb') as op_fh:
			op_config.write(op_fh)
		
		op_ok = True
		op_msg = 'Succeeded to apply storage account'

	except IOError as e:
		op_msg = 'Failed to access /root/.s3ql/authinfo2'
		log.error(str(e))
	except Exception as e:
		log.error(str(e))

	return_val = {'result' : op_ok,
		      'msg'    : op_msg,
                      'data'   : {}}

	log.info("apply_storage_account end")
	return json.dumps(return_val)

def apply_user_enc_key(old_key=None, new_key=None):
	log.info("apply_user_enc_key start")

	op_ok = False
	op_msg = 'Failed to change encryption keys for unexpetced errors.'

	try:
		#Check if the new key is of valid format
		if not common.isValidEncKey(new_key):
			op_msg = "New encryption Key has to an alphanumeric string of length between 6~20"		
			raise Exception(op_msg)

		op_config = ConfigParser.ConfigParser()
		if not os.path.exists('/root/.s3ql/authinfo2'):
			op_msg = "Failed to find authinfo2"
			raise Exception(op_msg)

       		with open('/root/.s3ql/authinfo2','rb') as op_fh:
			op_config.readfp(op_fh)

		section = "CloudStorageGateway"
		if not op_config.has_section(section):
			op_msg = "Section CloudStorageGateway is not found."
			raise Exception(op_msg)

		if op_config.has_option(section, 'bucket-passphrase'):
			key = op_config.get(section, 'bucket-passphrase')
			if key is not None and  key != old_key:
				op_msg = "old_key is not correct"
				raise Exception(op_msg)

		op_config.set(section, 'bucket-passphrase', new_key)
		with open('/root/.s3ql/authinfo2','wb') as op_fh:
			op_config.write(op_fh)
		
		op_ok = True
		op_msg = 'Succeeded to apply new user enc key'

	except IOError as e:
		op_msg = 'Failed to access /root/.s3ql/authinfo2'
		log.error(str(e))
	except Exception as e:
		log.error(str(e))
	finally:
		return_val = {'result' : op_ok,
			      'msg'    : op_msg,
			      'data'   : {}}

		log.info("apply_user_enc_key end")
		return json.dumps(return_val)

def build_gateway():
	return json.dumps(return_val)

def restart_nfs_service():
	log.info("restart_nfs_service start")

	return_val = {}
	op_ok = False
	op_msg = "Restarting nfs service failed."

	try:
		cmd = "/etc/init.d/nfs-kernel-server restart"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		po.wait()
		if po.returncode == 0:
			op_ok = True
			op_msg = "Restarting nfs service succeeded."
	except Exception as e:
		op_ok = False
		log.error(str(e))

	return_val = {
		'result': op_ok,
		'msg': op_msg,
		'data': {}
	}

	log.info("restart_nfs_service end")
	return json.dumps(return_val)

def restart_smb_service():
	log.info("restart_smb_service start")

	return_val = {}
	op_ok = False
	op_msg = "Restarting samba service failed."

	try:
		cmd = "/etc/init.d/smbd restart"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		po.wait()
		if po.returncode == 0:
			op_ok = True
			op_msg = "Restarting samba service succeeded."
	except Exception as e:
		op_ok = False
		log.error(str(e))

	return_val = {
		'result': op_ok,
		'msg': op_msg,
		'data': {}
	}

	log.info("restart_smb_service end")
	return json.dumps(return_val)

def reset_gateway():
	log.info("reset_gateway start")

	return_val = {}
	op_ok = True
	op_msg = "Succeeded to reset the gateway."
	
	pid = os.fork()
	if pid == 0:
		time.sleep(10)
		os.system("reboot")
	else:
		log.info("The gateway will restart after ten seconds.")
		return json.dumps(return_val)

def shutdown_gateway():
	log.info("shutdown_gateway start")
	
	return_val = {}
	op_ok = True
	op_msg = "Succeeded to shutdown the gateway."

	pid = os.fork()
	if pid == 0:
		time.sleep(10)
		os.system("poweroff")
	else:
		log.info("The gateway will shutdown after ten seconds.")
		return json.dumps(return_val)

@common.timeout(180)
def _test_storage_account(storage_url, account, password):
	cmd ="curl -k -v -H 'X-Storage-User: %s' -H 'X-Storage-Pass: %s' https://%s/auth/v1.0"%(account, password, storage_url)
	po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
	output = po.stdout.read()
        po.wait()

        if po.returncode != 0:
		op_msg = "Test storage account failed for %s"%output
               	raise TestStorageError(op_msg)

	if not common.isHttp200(output):
		op_msg = "Test storage account failed"
		raise TestStorageError(op_msg)

def test_storage_account(storage_url, account, password):
	log.info("test_storage_account start")

	op_ok = False
	op_msg = 'Test storage account failed for unexpetced errors.'

	try:
		_test_storage_account(storage_url=storage_url, account=account, password=password)
		op_ok = True
		op_msg ='Test storage account succeeded'
			
	except common.TimeoutError:
		op_msg ="Test storage account failed due to timeout" 
		log.error(op_msg)
	except TestStorageError as e:
		op_msg = str(e)
		log.error(op_msg)
	except Exception as e:
		log.error(str(e))
	finally:
		return_val = {'result' : op_ok,
			      'msg'    : op_msg,
			      'data'   : {}}

		log.info("test_storage_account end")
		return json.dumps(return_val)


if __name__ == '__main__':
	#Example of log usage
	log.debug("...")
	log.warn("...")
	log.info("...")
	log.error("...")
