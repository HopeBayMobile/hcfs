from django.shortcuts import render_to_response, redirect
from django.http import HttpResponse
import datetime

def index(request):
    """
    get account list
    """
    return render_to_response('index.html', {})
    
def server_time(request):
    """
    return current server time
    """
    #return server time
    now = datetime.datetime.now()
    return HttpResponse(now.strftime("%Y/%m/%d %H:%M"))

def disable_account(request):
    """
    """
    return HttpResponse("disable_account")
    #return redirect("/accounts/")

def enable_account(request):
    """
    """
    return HttpResponse("enable_account")
    #return redirect("/accounts/")

def new_account(request):
    """
    """
    return render_to_response('new_account.html', {})

def new_account_confirm(request):
    """
    """
    return HttpResponse("new_account_confirm")

def update_account(request):
    """
    """
    #return HttpResponse("update_account")
    return redirect("/accounts/")
    
def edit_account(request, id):
    """
    """
    return render_to_response('edit_account.html', {"id":id})

def get_password(request):
    """
    """
    return HttpResponse("get_password")

def reset_password(request):
    """
    """
    return HttpResponse("reset_password")

def disable_account(request):
    """
    """
    return HttpResponse("disable_account")

def enable_account(request):
    """
    """
    return HttpResponse("enable_account")

def new_user(request):
    """
    """
    return HttpResponse("new_user")

def new_user_confirm(request):
    """
    """
    return HttpResponse("new_user_confirm")