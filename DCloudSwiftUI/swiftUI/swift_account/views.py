from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse
import datetime

@login_required
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

@login_required
def disable_account(request, id):
    """
    """
    return HttpResponse("disable_account:"+id)
    #return redirect("/accounts/")

@login_required
def enable_account(request, id):
    """
    """
    return HttpResponse("enable_account:"+id)
    #return redirect("/accounts/")

@login_required
def new_account(request):
    """
    """
    return render_to_response('new_account.html', {})

@login_required
def new_account_confirm(request):
    """
    """
    return HttpResponse("new_account_confirm")

@login_required
def update_account(request, id):
    """
    """
    #return HttpResponse("update_account")
    return redirect("/accounts/")

@login_required
def edit_account(request, id):
    """
    """
    return render_to_response('edit_account.html', {"id":id})

@login_required
def get_password(request):
    """
    """
    return HttpResponse("get_password")

@login_required
def reset_password(request):
    """
    """
    return HttpResponse("reset_password")

@login_required
def disable_user(request, id):
    """
    """
    return HttpResponse("disable_user")

@login_required
def enable_user(request, id):
    """
    """
    return HttpResponse("enable_user")

@login_required
def new_user(request):
    """
    """
    return HttpResponse("new_user")

@login_required
def new_user_confirm(request):
    """
    """
    return HttpResponse("new_user_confirm")