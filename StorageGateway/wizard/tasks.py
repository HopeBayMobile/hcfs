from celery.task import task
import time

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
