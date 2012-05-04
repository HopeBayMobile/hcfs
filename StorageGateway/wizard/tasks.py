from celery.task import task
import time

@task
def form_1_task():
    pass

@task
def form_2_task():
    time.sleep(5)
    pass

@task
def form_3_task():
    time.sleep(5)
    pass

@task
def form_4_task():
    time.sleep(5)
    pass
