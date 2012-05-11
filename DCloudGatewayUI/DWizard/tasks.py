from celery.task import task

#@task
def task_1():
    print 'task_1'

#@task
def task_2():
    print 'task_2'

#@task
def task_3():
    print 'task_3'
