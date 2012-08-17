from celery.task import task
from celery.registry import tasks


def setup(api):
    try:
        for key in api:
            function = api[key]
            task_function = task(function)
            tasks.register(task_function)
    except:
        import sys
        import traceback
        print >> sys.stderr, traceback.format_exc()
