from django.contrib.auth.models import User
from celery.task import task
from lib.models.config import Config
import time

@task
def install_task():
    #Step 1, save password of administrator
    new_password = Config.objects.get(key='new_password')
    admin = User.objects.get(username='admin')
    admin.set_password(new_password.value)
    admin.save()
    #update status
    Config.objects.create(key='step_1', value="{'result': True, 'msg': 'string step_1'}") 
    
    #Step 2, apply network settings
    ip_address = Config.objects.get(key='ip_address')
    subnet_mask = Config.objects.get(key='subnet_mask')
    default_gateway = Config.objects.get(key='default_gateway')
    preferred_dns = Config.objects.get(key='preferred_dns')
    alternate_dns = Config.objects.get(key='alternate_dns')
    
    #call API: apply_network
    time.sleep(7)
    Config.objects.create(key='step_2', value="{'result': True, 'msg': 'string step_2'}") 
    
    #Step 3, cloud storage settings
    cloud_storage_ip = Config.objects.get(key='cloud_storage_ip')
    cloud_storage_account = Config.objects.get(key='cloud_storage_account')
    cloud_storage_password = Config.objects.get(key='cloud_storage_password')
    
    #call API: test_storage_account
    time.sleep(7)
    Config.objects.create(key='step_3', value="{'result': True, 'msg': 'string step_3'}") 
    
    #Step 4, encryption key
    encryption_key = Config.objects.get(key='encryption_key')
    
    #call API: apply_user_enc_key
    time.sleep(7)
    Config.objects.create(key='step_4', value="{'result': True, 'msg': 'string step_4'}") 
    time.sleep(7)
    return True

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
