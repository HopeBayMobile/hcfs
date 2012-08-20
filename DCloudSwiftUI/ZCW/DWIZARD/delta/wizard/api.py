from celery.task import Task
from celery import states

from django.conf import settings
import urllib
import os.path
import json

""" Write your own task as the following:

from delta.wizard.api import DeltaWizardTask

@task(base=DeltaWizardTask)
def my_task(data):

    #report task progress by using wizard api
    my_task.report_progress(10, True, 'My Progress 1', None)
    #do something
    my_task.report_progress(100, True, 'My Progress 2', None)
"""


class DeltaWizardTask(Task):
    abstract = True

    meta = {}

    def __init__(self):
        self.meta = {'step_list': [], 'progress': 0}

    def report_progress(self, percentage, result, msg, data):
        """Report task progress
        :param percentage: value between 0 to 100
        :param result: boolean for executing result
        :param msg: displaying message
        :param data: return data after executing
        """
        self.meta['progress'] = percentage

        response = {}
        response['result'] = result
        response['msg'] = msg
        response['data'] = data
        self.meta['step_list'].append(response)

        if result:
            #self.update_state(state=states.STARTED, meta=self.meta)
            self.backend.store_result(self.request.id, self.meta, states.STARTED)
        else:
            # TODO: Add an exception type for the failed task.
            raise Exception

    def after_return(self, status, retval, task_id, args, kwargs, einfo):
        """Handler called after the task returns.

        :param status: Current task state.
        :param retval: Task return value/exception.
        :param task_id: Unique id of the task.
        :param args: Original arguments for the task that failed.
        :param kwargs: Original keyword arguments for the task
                       that failed.

        :keyword einfo: :class:`~celery.datastructures.ExceptionInfo`
                        instance, containing the traceback (if any).

        The return value of this handler is ignored.
        """
        #write meta data into backend

        if isinstance(retval, bool):
            if retval:
                self.meta['progress'] = 100.0
                self.backend.store_result(task_id, self.meta, states.SUCCESS)
            else:
                self.backend.store_result(task_id, self.meta, states.FAILURE)
        else:
            if status == states.SUCCESS:
                self.meta['progress'] = 100.0
                self.backend.store_result(task_id, self.meta, status)
            elif status == states.FAILURE:
                response = {}
                response['result'] = False
                response['msg'] = retval
                response['data'] = None
                self.meta['step_list'].append(response)
                self.backend.store_result(task_id, self.meta, status)

        #Clean meta to prevent showing in next step
        #Because all tasks use the same class, every task only one instance...?
        self.meta = {'step_list': [], 'progress': 0}

        #trigger wizard to report wizard progress ,no data need to add
        wizard_notification_url = getattr(settings, 'WIZARD_NOTIFICATION_URL', 'http://127.0.0.1:8765/wizard/notify')
        try:
            response = urllib.urlopen(wizard_notification_url)
        except:
            pass

    def get_zone_hosts(self):
        """ Read /var/zcw/hosts file to get host list in the zone
        The example of hosts file content:
        [
        {"hostname":"TPE1AA0101","island":"AA","rack":1},
        {"hostname":"TPE1AA0102","island":"AA","rack":1}
        ]
        """
        host_list = []
        path = '/var/zcw/hosts'
        if os.path.exists(path):
            with open(path) as f:
                host_list = json.loads(f.read())
            return host_list
        else:
            return host_list
