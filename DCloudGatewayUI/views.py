from django.shortcuts import render, redirect
from django.contrib import auth
from lib.models.config import Config
from wizard.urls import InstallWizard


def home(request):
    if Config.objects.all().count() == 0:
        return redirect('/wizard/welcome')
    elif InstallWizard.is_going():
        return redirect('/wizard/')
    elif request.user.is_authenticated():
        return redirect('/dashboard')
    else:
        return render(request, 'home.html')


def login(request):
    if request.method == 'GET':
        return render(request, 'home.html')
    elif request.method == 'POST':
        username = request.POST['username']
        password = request.POST['password']
        user = auth.authenticate(username=username, password=password)

        if user is not None:
            auth.login(request, user)
            return redirect('/dashboard')
        else:
            return render(request, 'home.html', {'error_msg': 'Incorrect user\
                name or password'})


def logout(request):
    auth.logout(request)
    return render(request, 'home.html')
