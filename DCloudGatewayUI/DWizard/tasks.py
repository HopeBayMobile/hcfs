from celery.task import task
import time

@task
def task_1(data):
    print 'task_1'
    time.sleep(7)
    return {'result': True,
            'msg': 'task 1',
            'data': {}
            }

@task
def task_2(data):
    print 'task_2'
    time.sleep(7)
    return {'result': True,
            'msg': 'task 2',
            'data': {}
            }

@task
def task_3(data):
    print 'task_3'
    time.sleep(7)
    return {'result': True,
            'msg': 'task 3',
            'data': {}
            }
