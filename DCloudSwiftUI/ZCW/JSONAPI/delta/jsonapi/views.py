from django.conf import settings
from django.shortcuts import HttpResponse, render, redirect
from django.views.decorators.http import require_POST
from django.views.decorators.csrf import csrf_exempt

from celery.task import task
from celery.result import AsyncResult

import json
import inspect
"""Use Delta Json API by add the following code in setting.py:

ZONE_API = {
    'your_api_name': your_api_function_object
}
from delta import jsonapi
jsonapi.setup(ZONE_API)

"""


@csrf_exempt
@require_POST
def call(request):
    '''
    /api/call

    this will call the function that is set in settings.py
    under ZONE_API with incoming input format as follow:

    raw_post_data = {
        'function': 'start_vm', # function name to call
        'params': {
            'user_id': '1234',
            'flavor': 'micro',
            ...
        }
    }
    '''
    call = json.loads(request.body)
    function_name = call['function']
    function_params = call['params']

    zone_api = settings.ZONE_API

    response = HttpResponse(content_type='application/json')
    reply = {}
    reply['result'] = False
    reply['data'] = None

    #check function
    if function_name in zone_api:
        function = zone_api[function_name]

        task_obj = task(function)
        argspec = inspect.getargspec(function)

        #check all params
        if set(function_params) == set(argspec.args):
            # rearrange the argument in correct order
            call_args = [function_params[arg] for arg in argspec.args]

            result = task_obj.apply_async(call_args)

            reply['result'] = True
            reply['msg'] = None
            reply['data'] = {'id': str(result.task_id)}
        else:
            reply['msg'] = str(function_name) + ' miss required argument'
    else:
        reply['msg'] = str(function_name) + ' does not exist in ZONE_API'

    response.content = json.dumps(reply)
    return response


def list(req):
    '''
    /api/list

    this will list all the available api that is set in settings.py
    under ZONE_API dictionary with following format:

    from openstack_api import start_vm

    ZONE_API = {
        'start_vm': start_vm
    }

    the dictionary key is the function name for the correspond api call params
    e.g. /api/call -> POST data
        {
            'function': 'start_vm'
            ...
        }

    the return is in json format as follow example shows:
    {
        'listVirtualMachines: { # function name
            'account': { # parameter name
                'required': True, # false if has default value
                'description': 'account id for VM owner'
            }...
        }
    }
    '''
    api_list = settings.ZONE_API
    json_api_list = {}

    for func_name in api_list:
        api_spec = {}

        function = api_list[func_name]
        argspec = inspect.getargspec(function)
        default = argspec.args[-len(argspec.defaults):]
        for arg in argspec.args:
            api_spec[arg] = {}
            if arg in default:
                api_spec[arg]['required'] = False
            else:
                api_spec[arg]['required'] = True

        json_api_list[func_name] = api_spec

    return json_api_list


def task_status(req, task_id):
    task_result = AsyncResult(task_id)

    resp = {}

    resp['result'] = True
    resp['data'] = {
        'id': task_id,
        'status': task_result.status,
        'result': {
            'data': task_result.result
        }
    }

    return resp


def task_cancel(req, task_id):
    pass
