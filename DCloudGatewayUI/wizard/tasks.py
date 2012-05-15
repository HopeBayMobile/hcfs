from django.contrib.auth.models import User
from celery.task import task
from lib.models.config import Config
import time

@task
def install_task(data):
    #save data in lib_config table
    for k, v in data.iteritems():
        c = Config(key=k, value=v)
        c.save()
    
    #Step 1, save password of administrator
    new_password = Config.objects.get(key='new_password')
    admin = User.objects.get(username='admin')
    admin.set_password(new_password.value)
    admin.save()
        
    #Step 2, apply network settings
    ip_address = Config.objects.get(key='ip_address')
    subnet_mask = Config.objects.get(key='subnet_mask')
    default_gateway = Config.objects.get(key='default_gateway')
    preferred_dns = Config.objects.get(key='preferred_dns')
    alternate_dns = Config.objects.get(key='alternate_dns')
    
    #call API: apply_network
        
    #Step 3, cloud storage settings
    cloud_storage_url = Config.objects.get(key='cloud_storage_url')
    cloud_storage_account = Config.objects.get(key='cloud_storage_account')
    cloud_storage_password = Config.objects.get(key='cloud_storage_password')
    
    #call API: test_storage_account
    
    #Step 4, encryption key
    encryption_key = Config.objects.get(key='encryption_key')
    
    #call API: apply_user_enc_key
    
    
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
