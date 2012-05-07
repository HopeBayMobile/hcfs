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

def apply_network(input_value):
    
    input_pack = json.loads(input_value)

    print('received ip: %s' % input_pack['ip'])
    print('received gateway: %s' % input_pack['gateway'])
    print('received mask: %s' % input_pack['mask'])    
    print('received dns1: %s' % input_pack['dns1'])   
    if input_pack['dns2'] == None:
        print('There is no received dns2')
    else:
        print('received dns2: %s' % input_pack['dns2']) 


def get_storage_account():

    return_val = {'result' : True,
                  'msg' : 'This is a mock value for testing purpose',
                  'data' : {'storage_url' : 'cloudgw.delta.com.tw:8080',
                            'account' : 'system:root'}}
    return json.dumps(return_val)

def test_storage_account(input_value):

    input_pack = json.loads(input_value)

    print('received storage_url: %s' % input_pack['storage_url'])
    print('received account: %s' % input_pack['account'])
    print('received password: %s' % input_pack['password'])

def apply_storage_account(input_value):

    input_pack = json.loads(input_value)

    print('received storage_url: %s' % input_pack['storage_url'])
    print('received account: %s' % input_pack['account'])
    print('received password: %s' % input_pack['password'])
    try:
        if input_pack['test'] == True:
            print('Need to test account')
        else:
            print('No need to test account')
    except KeyError:
        print('Need to test account')

def apply_user_enc_key(input_value):

    input_pack = json.loads(input_value)

    print('received old_key: %s' % input_pack['old_key'])
    print('received new_key: %s' % input_pack['new_key'])

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
