import json

# Assuming all inputs to the functions are JSON objects

def get_network():

    return_val = {'result' : True,
                  'msg' : 'This is a mock value for testing purpose',
                  'data' : {'ip' : '172.16.228.200',
                            'gateway' : '172.16.228.1',
                            'mask' : '255.255.255.0',
                            'dns1' : '172.16.228.100',
                            'dns2' : None}}
    return json.dumps(return_val)

def apply_network(ip, gateway, mask, dns1, dns2=None):
    
    print('received ip: %s' % ip)
    print('received gateway: %s' % gateway)
    print('received mask: %s' % mask)    
    print('received dns1: %s' % dns1)   
    if dns2 == None:
        print('There is no received dns2')
    else:
        print('received dns2: %s' % dns2) 


def get_storage_account():

    return_val = {'result' : True,
                  'msg' : 'This is a mock value for testing purpose',
                  'data' : {'storage_url' : 'cloudgw.delta.com.tw:8080',
                            'account' : 'system:root'}}
    return json.dumps(return_val)

def test_storage_account(storage_url, account, password):

    print('received storage_url: %s' % storage_url)
    print('received account: %s' % account)
    print('received password: %s' % password)

def apply_storage_account(storage_url, account, password, test = True):

    print('received storage_url: %s' % storage_url)
    print('received account: %s' % account)
    print('received password: %s' % password)
    if test == True:
        print('Need to test account')
    else:
        print('No need to test account')

def apply_user_enc_key(old_key, new_key):

    print('received old_key: %s' % old_key)
    print('received new_key: %s' % new_key)

def build_gateway():
	return_val = {
		'result': True,
		'msg' : "This is a mock value for testing purpose",
		'data': {}
	}
	return json.dumps(return_val)

def restart_nfs_service():
	return_val = {
		'resutl': True,
		'msg': "This is a mock value for testing purpose.",
		'data': {}
	}
	return json.dumps(return_val)

def restart_smb_service():
	return_val = {
		'result': True,
		'msg': "This is a mock value for testing purpose.",
		'data': {}
	}
	return json.dumps(return_val)

def reset_gateway():
	return_val = {
		'result': True,
		'msg': "This is a mock value for testing purpose.",
		'data': {}
	}
	return json.dumps(return_val)

def shutdown_gateway():
	return_val = {
		'result': True,
		'msg': "This is a mock value for testing purpose.",
		'data': {}
	}
	return json.dumps(return_val)
