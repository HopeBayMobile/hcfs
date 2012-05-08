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

    return_val = {
            'result': True,
            'msg' : "This is a mock value for testing purpose",
            'data': {}
    }
    return json.dumps(return_val)


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

    return_val = {
            'result': True,
            'msg' : "This is a mock value for testing purpose",
            'data': {}
    }
    return json.dumps(return_val)

def apply_storage_account(storage_url, account, password, test = True):

    print('received storage_url: %s' % storage_url)
    print('received account: %s' % account)
    print('received password: %s' % password)
    if test == True:
        print('Need to test account')
    else:
        print('No need to test account')

    return_val = {
            'result': True,
            'msg' : "This is a mock value for testing purpose",
            'data': {}
    }
    return json.dumps(return_val)


def apply_user_enc_key(old_key, new_key):

    print('received old_key: %s' % old_key)
    print('received new_key: %s' % new_key)

    return_val = {
            'result': True,
            'msg' : "This is a mock value for testing purpose",
            'data': {}
    }
    return json.dumps(return_val)


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

def get_scheduling_rules():
	
	return_val = {
		'result': True,
		'msg': "This is a mock value for testing purpose.",
		'data': {'policies' : [{'day' : 0, 'start' : -1, 'stop' : -1},
                                       {'day' : 1, 'start' : 2, 'stop' : 5}]}
	}
	return json.dumps(return_val)

def apply_scheduling_rules( policies ):

        for entry in policies:
            print('received values: day %d, start %d, stop %d' %
                  (entry['day'], entry['start'], entry['stop']))
	
	return_val = {
		'result': True,
		'msg': "This is a mock value for testing purpose.",
		'data': {}
	}
	return json.dumps(return_val)

def get_gateway_indicators():

    return_val = {'result' : True,
                       'msg'    : 'This is a mock value for testing purpose',
                       'data'   : {'network_ok' : True,
                                     'system_check' : True,
                                     'flush_inprogress' : True,
                                     'dirtycache_nearfull' : True,
                                     'HDD_ok' : True,
                                     'NFS_srv' : True,
                                     'SMB_srv' : True}}

    return json.dumps(return_val)
