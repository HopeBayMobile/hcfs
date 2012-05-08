from django.shortcuts import render, redirect
from django.contrib import auth
from lib.models.config import Config

def home(request):
    if Config.objects.all().count() == 0 :
        return redirect('/wizard/welcome')
    elif Config.objects.filter(key='wizard').count() == 1:
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
            return render(request, 'home.html')

def logout(request):
    auth.logout(request)
    return render(request, 'home.html')
    
