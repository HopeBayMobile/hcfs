import json
import os
import ConfigParser

def get_storage_account():

    op_ok = False
    op_msg = 'Storage account read failed unexpectedly.'
    op_storage_url = ''
    op_account = ''

    try:
        op_fh = open('/root/.s3ql/authinfo2','r')
    except IOError:
        op_msg = 'Storage account information not created or not readable.'
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
    
    return_val = {'result' : op_ok,
                  'msg' : op_msg,
                  'data' : {'storage_url' : op_storage_url,
                            'account' : op_account}}
    return json.dumps(return_val)

def build_gateway():
	return json.dumps(return_val)

def restart_nfs_service():
	return_val = {}
	op_ok = False
	op_msg = "Restarting the nfs service failed."

	try:
		cmd = "/etc/init.d/nfs-kernel-server restart"
		po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		po.wait()
		if po.returncode == 0:
			op_ok = True
			op_msg = "Restarting the nfs service succeeded."
	except Exception as e:
		op_ok = False
		op_msg = str(e)
	finally:
		return_val = {
			'result': op_ok,
			'msg': op_msg,
			'data': {}
		}
		return json.dumps(return_val)

def restart_smb_service():
	return json.dumps(return_val)

def reset_gateway():
	return json.dumps(return_val)

def shutdown_gateway():
	return json.dumps(return_val)
