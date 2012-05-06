import json

def get_storage_account():

    return_val = {'result' : True,
                  'msg' : 'This is a mock value for testing purpose',
                  'data' : {'storage_url' : 'cloudgw.delta.com.tw:8080',
                            'account' : 'system:root'}}
    return json.dumps(return_val)

