from django.contrib.auth.models import User
from celery.task import task
from celery import states
from lib.models.config import Config
import time
from gateway import api
import json

@task
def install_task(data):
    meta = {'step_list':[]}
    step_list = meta['step_list']
    
    #save data in lib_config table
    for k, v in data.iteritems():
        c = Config(key=k, value=v)
        c.save()
    #Step 1, save password of administrator
    new_password = data['new_password']
    admin = User.objects.get(username='admin')
    admin.set_password(new_password)
    admin.save()
    
    response_str = '{"result":true, "msg":"Password changed successful.","data":{} }'
    response = json.loads(response_str)
    step_list.append(response)
    
    install_task.update_state(state=states.STARTED, meta=meta)

    #Step 2, apply network settings
    ip_address = data['ip_address']
    subnet_mask = data['subnet_mask']
    default_gateway = data['default_gateway']
    preferred_dns = data['preferred_dns']
    alternate_dns = data['alternate_dns']
    
    #call API: apply_network    
    response_str = api.apply_network(ip_address, default_gateway, subnet_mask, preferred_dns, alternate_dns)
    response = json.loads(response_str)
    step_list.append(response)    
    
    if response['result']:        
        install_task.update_state(state=states.STARTED, meta=meta)
    else:
        install_task.update_state(state=states.FAILURE, meta=meta)
        return meta

    #Step 3, cloud storage settings
    cloud_storage_url = data['cloud_storage_url']
    cloud_storage_account = data['cloud_storage_account']
    cloud_storage_password = data['cloud_storage_password']
    
    #call API: test_storage_account
    response_str = api.apply_storage_account(cloud_storage_url, cloud_storage_account, cloud_storage_password)
    response = json.loads(response_str)
    step_list.append(response)       

    if response['result']:        
        install_task.update_state(state=states.STARTED, meta=meta)
    else:
        install_task.update_state(state=states.FAILURE, meta=meta)
        return meta

    #Step 4, encryption key
    encryption_key = data['encryption_key']
    
    #call API: apply_user_enc_key
    response_str = api.build_gateway(encryption_key)
    response = json.loads(response_str)
    step_list.append(response)
    
    if response['result']:        
        install_task.update_state(state=states.STARTED, meta=meta)
    else:
        install_task.update_state(state=states.FAILURE, meta=meta)
        return meta
    
    #end of installation
    return meta

@task
def step_1_task():
    time.sleep(7)
    return {'result': True,
            'msg': 'Step 1',
            'data': {}
            }

@task
def step_2_task():
    time.sleep(7)
    return {'result': False,
            'msg': 'Step 2',
            'data': {}
            }

@task
def step_3_task():
    time.sleep(7)
    return {'result': True,
            'msg': 'Step 3',
            'data': {}
            }

@task
def step_4_task():
    time.sleep(7)
    return {'result': True,
            'msg': 'Step 4',
            'data': {}
            }
