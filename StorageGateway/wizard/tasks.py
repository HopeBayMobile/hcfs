from celery.task import task
from lib.models.config import Config
import time

@task
def install_task():
    time.sleep(7)
    Config.objects.create(key='step_1', value="{'result': True, 'msg': 'string step_1'}") 
    time.sleep(7)
    Config.objects.create(key='step_2', value="{'result': True, 'msg': 'string step_2'}") 
    time.sleep(7)
    Config.objects.create(key='step_3', value="{'result': True, 'msg': 'string step_3'}") 
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
