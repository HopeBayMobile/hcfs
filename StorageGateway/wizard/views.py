from django.shortcuts import render
from django.core.exceptions import ObjectDoesNotExist
from django.contrib.auth import authenticate
from django.contrib.auth.models import User
from lib.models.config import Config
from forms import Form_1, Form_2, Form_3, Form_4, Form_All
from tasks import step_1_task, step_2_task, step_3_task, step_4_task
from celery.result import AsyncResult
from celery import states

#define constant
STEP = "step_0", "step_1", "step_2", "step_3", "step_4"

def welcome(request):
    return render(request, 'welcome.html')

def index(request):
    #get status of wizard
    try:
        wizard = Config.objects.get(key='wizard')
    except ObjectDoesNotExist:
        return form_action(request)
    
    #determine the current step
    step_name = wizard.value
    try:
        step_task = Config.objects.get(key=step_name)
    except ObjectDoesNotExist:
        step_task = None
    
    if step_task:
        #check task status
        result = AsyncResult(step_task.value)
        if result.state == states.SUCCESS:
            if result.result['result']:
                
                #next step
                wizard = Config.objects.get(key='wizard')
                wizard.value = 'step_' + str(int(step_name[-1]) + 1)
                wizard.save()
                return render(request, 'process.html', {'info': 'Finish ' + step_name})
            else:
                #clear step
                Config.objects.get(key='wizard').delete()
                Config.objects.filter(key__startswith='step_').delete()  
                return render(request, 'message.html', {'error': result.result['msg']})
        else:
            return render(request, 'process.html', {'info': step_name + ' is running'})
    else:
        if step_name == 'step_5':
            #installation complete
            return render(request, 'home.html')
        else:
            #execute task
            result = eval(step_name + "_task.delay()")
            step_task = Config.objects.create(key=step_name, value=result.task_id)
            return render(request, 'process.html', {'info': 'Start ' + step_name})


def form_action(request):
    if request.method == 'POST':
        form = Form_All(request.POST)
        if form.is_valid():
            #Save settings into lib_config
            for field in form.fields:
                c = Config(key=field, value=form.data[field])
                c.save()
            
            #Save status of wizard
            Config.objects.create(key='wizard', value='step_1')
                        
            return render(request, 'process.html', {'info': 'Save settings and execute setup.'})
    else:
        form = Form_All()
    
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
                return render(request, 'message.html', {'error': 'Username or password were incorrect.'})
            
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
