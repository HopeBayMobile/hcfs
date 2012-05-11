from django.shortcuts import render
from django.core.exceptions import ObjectDoesNotExist
from django.contrib.auth import authenticate
from django.contrib.auth.models import User
from lib.models.config import Config
from forms import Form_1, Form_2, Form_3, Form_4, Form_All
from tasks import step_1_task, step_2_task, step_3_task, step_4_task, install_task
from celery.result import AsyncResult
from celery import states

#define constant
STEP = "step_0", "step_1", "step_2", "step_3", "step_4"

def welcome(request):
    return render(request, 'welcome.html')

def index(request):
    #get status of wizard
    try:
        install_task = Config.objects.get(key='wizard')
    except ObjectDoesNotExist:
        install_task = None
    
    if install_task:
        #check task status
        result = AsyncResult(install_task.value)
        if result.state == states.SUCCESS:
            if result.result:
                Config.objects.get(key='wizard').delete()
                return render(request, 'message.html', {'info': 'Installation Completed.',
                                                        'next': '/'})
            else:
                #clear step
                Config.objects.get(key='wizard').delete()
                Config.objects.filter(key__startswith='step_').delete()  
                return render(request, 'message.html', {'error': 'Installation Failed.',
                                                        'next': '.'})
        else:
            steps = Config.objects.filter(key__startswith='step_').order_by('-key')
            return render(request, 'progress.html', {'steps': steps})
    else:
        return form_action(request)


def form_action(request):
    initial_data = {'ip_address':'192.168.0.1',
                    'subnet_mask':'255.255.255.0',
                    'default_gateway':'192.168.0.254',
                    'preferred_dns':'8.8.8.8',
                    'alternate_dns':'8.8.4.4',
                    
                    'cloud_storage_url':'http://delta.cloud.storage:8080/'
                    }
    if request.method == 'POST':
        form = Form_All(request.POST)
        if form.is_valid():
            #Save settings into lib_config
            for field in form.fields:
                c = Config(key=field, value=form.cleaned_data[field])
                c.save()
            #execute install_task and save task_id
            result = install_task.delay()
            Config.objects.create(key='wizard', value=result.task_id)    
            return render(request, 'process.html', {'info': 'Save settings and execute setup.'})
    else:
        form = Form_All(initial=initial_data)
    
    return render(request, 'form.html', {'header': 'System Installation',
                                         'form': form,
                                         'action': '.',
                                         'submit': 'Install',
                                         })

def wizard_index(request):
    try:
        wizard = Config.objects.get(key='wizard')
    except ObjectDoesNotExist:
        wizard = Config.objects.create(key='wizard', value=STEP[0])
                  
    case = {STEP[0]: wizard_1_action,
            STEP[1]: wizard_2_action,
            STEP[2]: wizard_3_action,
            STEP[3]: wizard_4_action,
            STEP[4]: finish_action,
            }
    
    return case[wizard.value](request)

def wizard_1_action(request):    
    if request.method == 'POST':
        form = Form_1(request.POST)
        if form.is_valid():
            #Verify user and change admin password
            user = authenticate(username=form.cleaned_data['username'], password=form.cleaned_data['password'])
            if user is not None:
                admin = User.objects.get(username=form.cleaned_data['username'])
                admin.set_password(form.cleaned_data['new_password'])
                admin.save()
            else:
                return render(request, 'message.html', {'error': 'Username or password were incorrect.',
                                                        'next': '.'})
            
            #Save status of wizard
            wizard = Config.objects.get(key='wizard')
            wizard.value = STEP[1]
            wizard.save()
            #return render(request, 'process.html')
            form = Form_2()
    else:
        form = Form_1()
    
    return render(request, 'form.html', {'header': 'System Installation',
                                         'form': form,
                                         'action': '.',
                                         'submit': 'Next',
                                         })

def wizard_2_action(request):
    try:
        step_task = Config.objects.get(key=STEP[2])
    except ObjectDoesNotExist:
        step_task = None
    
    if step_task:
        result = AsyncResult(step_task.value)
        if result.state == states.SUCCESS:
            #Save status of wizard
            wizard = Config.objects.get(key='wizard')
            wizard.value = STEP[2]
            wizard.save()
            return render(request, 'process.html')
        else:
            return render(request, 'process.html')
    
    if request.method == 'POST':
        form = Form_2(request.POST)
        if form.is_valid():
            result = step_2_task.delay()
            step_task = Config.objects.create(key=STEP[2], value=result.task_id)            
            return render(request, 'process.html')
    else:
        form = Form_2()

    return render(request, 'form.html', {'header': 'System Installation',
                                         'form': form,
                                         'action': '.',
                                         'submit': 'Next',
                                         })

def wizard_3_action(request):
    try:
        step_task = Config.objects.get(key=STEP[3])
    except ObjectDoesNotExist:
        step_task = None
    
    if step_task:
        result = AsyncResult(step_task.value)
        if result.state == states.SUCCESS:
            #Save status of wizard
            wizard = Config.objects.get(key='wizard')
            wizard.value = STEP[3]
            wizard.save()
            return render(request, 'process.html')
        else:
            return render(request, 'process.html')
        
    if request.method == 'POST':
        form = Form_3(request.POST)
        if form.is_valid():
            result = step_3_task.delay()
            step_task = Config.objects.create(key=STEP[3], value=result.task_id)            
            return render(request, 'process.html')
    else:
        form = Form_3()

    return render(request, 'form.html', {'header': 'System Installation',
                                         'form': form,
                                         'action': '.',
                                         'submit': 'Next',
                                         })

def wizard_4_action(request):
    try:
        step_task = Config.objects.get(key=STEP[4])
    except ObjectDoesNotExist:
        step_task = None
    
    if step_task:
        result = AsyncResult(step_task.value)
        if result.state == states.SUCCESS:
            #Save status of wizard
            wizard = Config.objects.get(key='wizard')
            wizard.value = STEP[4]
            wizard.save()
            return render(request, 'process.html')
        else:
            return render(request, 'process.html')
    
    if request.method == 'POST':
        form = Form_4(request.POST)
        if form.is_valid():
            result = step_4_task.delay()
            step_task = Config.objects.create(key=STEP[4], value=result.task_id)            
            return render(request, 'process.html')
    else:
        form = Form_4()

    return render(request, 'form.html', {'header': 'System Installation',
                                         'form': form,
                                         'action': '.',
                                         'submit': 'Next',
                                         })

def finish_action(request):
    return render(request, 'home.html')
