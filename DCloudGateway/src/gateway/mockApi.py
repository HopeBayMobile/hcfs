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
		'data': {'policies' : [{'day' : 2, 'start' : -1, 'stop' : -1, 'uplink_bw_limit' : 100},
                                       {'day' : 3, 'start' : 2, 'stop' : 5, 'uplink_bw_limit': 200}]}
	}
	return json.dumps(return_val)

def apply_scheduling_rules( policies ):

        for entry in policies:
            print('received values: day %d, start %d, stop %d, bandwidth limit %d' %
                  (entry['day'], entry['start'], entry['stop'], entry['uplink_bw_limit']))
	
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

def get_gateway_status():



    return_val = { 'result' : True,
                         'msg' : 'This is a mock value for testing purpose',
                         'data' : { 'error_log' : [{ 'category' : 'SMB', 
                                                    'timestamp' : '2012-04-24 14:26:25.749', 
                                                    'msg' : 'smb service start error'}],
                                        'cloud_storage_usage' : { 'cloud_data' : 1000,
                                               'cloud_data_dedup' : 800,
                                               'cloud_data_dedup_compress' : 650 
                                             },
                                        'gateway_cache_usage' : { 'max_cache_size' : 200,
                                                                  'max_cache_entries' : 250000,
                                                        'used_cache_size' : 180.5,
                                                        'used_cache_entries' : 20000,
                                                        'dirty_cache_size' : 160,
                                                        'dirty_cache_entries' : 18000
                                                       },
                                        'uplink_usage' : 100,
                                        'downlink_usage' : 1000,
                                        'uplink_backend_usage' : 80,
                                        'downlink_backend_usage' : 600
					  }}

    return json.dumps(return_val)

def get_compression():
    return_val = {
            'result': True,
            'msg': "This is a mock value for testing purpose.",
            'data': {'switch' : True}
    }
    return json.dumps(return_val)


def set_compression(switch):
    if switch:
        print('Turning compression on')
    else:
        print('Turning compression off') 

    return_val = {
            'result': True,
            'msg': "This is a mock value for testing purpose.",
            'data': {}
    }
    return json.dumps(return_val)

def get_gateway_system_log():
    #TODO

    return_val = {
            'result': True,
            'msg': "This is a mock value for testing purpose.",
            'data': {}
    }
    return json.dumps(return_val)

def get_smb_user_list():
    #TODO

    return_val = {
            'result': True,
            'msg': "This is a mock value for testing purpose.",
            'data': {}
    }
    return json.dumps(return_val)

def set_smb_user_list():
    #TODO

    return_val = {
            'result': True,
            'msg': "This is a mock value for testing purpose.",
            'data': {}
    }
    return json.dumps(return_val)

def get_nfs_access_ip_list():
    #TODO

    return_val = {
            'result': True,
            'msg': "This is a mock value for testing purpose.",
            'data': {}
    }
    return json.dumps(return_val)

def set_nfs_access_ip_list():
    #TODO

    return_val = {
            'result': True,
            'msg': "This is a mock value for testing purpose.",
            'data': {}
    }
    return json.dumps(return_val)

def stop_upload_sync():
    #TODO

    return_val = {
            'result': True,
            'msg': "This is a mock value for testing purpose.",
            'data': {}
    }
    return json.dumps(return_val)

def force_upload_sync():
    #TODO

    return_val = {
            'result': True,
            'msg': "This is a mock value for testing purpose.",
            'data': {}
    }
    return json.dumps(return_val)

