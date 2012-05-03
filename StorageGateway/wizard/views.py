from django.shortcuts import render
from django.core.exceptions import ObjectDoesNotExist
from django.contrib.auth import authenticate
from django.contrib.auth.models import User
from lib.models.config import Config
from forms import Form_1, Form_2, Form_3, Form_4

def welcome(request):
    return render(request, 'welcome.html')

def index(request):
    try:
        wizard = Config.objects.get(key='wizard')
    except ObjectDoesNotExist:
        wizard = Config.objects.create(key='wizard', value=str(None))
                  
    case = {str(None): form_1_action,
            Form_1.__name__: form_2_action,
            Form_2.__name__: form_3_action,
            Form_3.__name__: form_4_action,
            Form_4.__name__: finish_action,
            }
    
    return case[wizard.value](request)

def form_1_action(request):
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
            wizard.value = Form_1.__name__
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

def form_2_action(request):
    if request.method == 'POST':
        form = Form_2(request.POST)
        if form.is_valid():
            
            #Save status of wizard
            wizard = Config.objects.get(key='wizard')
            wizard.value = Form_2.__name__
            wizard.save()
            return render(request, 'process.html')
    else:
        form = Form_2()

    return render(request, 'form.html', {'header': 'System Installation',
                                         'form': form,
                                         'action': '.',
                                         'submit': 'Next',
                                         })

def form_3_action(request):
    if request.method == 'POST':
        form = Form_3(request.POST)
        if form.is_valid():
            
            #Save status of wizard
            wizard = Config.objects.get(key='wizard')
            wizard.value = Form_3.__name__
            wizard.save()
            return render(request, 'process.html')
    else:
        form = Form_3()

    return render(request, 'form.html', {'header': 'System Installation',
                                         'form': form,
                                         'action': '.',
                                         'submit': 'Next',
                                         })

def form_4_action(request):
    if request.method == 'POST':
        form = Form_4(request.POST)
        if form.is_valid():
            
            #Save status of wizard
            wizard = Config.objects.get(key='wizard')
            wizard.value = Form_4.__name__
            wizard.save()
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
