from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse
import datetime

@login_required
def index(request):
    """
    get account list
    """
    return render_to_response('list_account.html', {})

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
    if "account_id" in request.POST:
        account_id = request.POST["account_id"]
        description = request.POST["description"]
        #apply
        return render_to_response('confirm_account.html', 
          {"account_id":account_id,
           "description":description,
           "identity":"Administrator",
           "Password":"NFFG457dSC8056B"})
    else:
        return HttpResponse("new_account_confirm")

@login_required
def process_account(request):
    """
    """
    #return HttpResponse("process_account")
    return redirect("/accounts/")

@login_required
def edit_account(request, id):
    """
    """
    return render_to_response('edit_account.html',
      {"account_id":id, 
       "description":"Long long story......"})

@login_required
def update_account(request, id):
    """
    """
    #return HttpResponse("update_account")
    return redirect("/accounts/")

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
def new_user(request, id):
    """
    """
    #return HttpResponse("new_user")
    return render_to_response('new_user.html', {"account_id":id})

@login_required
def new_user_confirm(request, id):
    """
    """
    if "user_id" in request.POST:
        user_id = request.POST["user_id"]
        description = request.POST["description"]
        return render_to_response('confirm_user.html',
          {"account_id":id,
           "user_id":user_id,
           "description":description,
           "Password":"NFFG457dSC8056B"})
    return HttpResponse("new_user_confirm")

@login_required
def process_user(request, id):
    """
    """
    return redirect("/accounts/")

@login_required
def edit_user(request, id, user_id):
    """
    """
    #return HttpResponse("edit_user")
    return render_to_response('edit_user.html',
      {"account_id":id, 
       "user_id":user_id,
       "description":"Long long story......"})

@login_required
def update_user(request, id, user_id):
    """
    """
    return HttpResponse("update_user")

@login_required
def delete_user(request, id, user_id):
    """
    """
    return HttpResponse("delete_user")