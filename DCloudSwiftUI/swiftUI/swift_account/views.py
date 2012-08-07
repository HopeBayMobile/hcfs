from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse
import datetime
from DCloudSwift.master.swiftAccountMgr import SwiftAccountMgr

@login_required
def index(request):
    """
    get account list
    """
    SA = SwiftAccountMgr()
    result = SA.list_account()
    if result.val:
        accounts = result.msg
        account_list = [];
        for i in accounts:
            accounts[i]["id"]=i
            account_list.append(accounts[i])
        return render_to_response('list_account.html', {"accounts":account_list,
            "request": request })
    else:
        return HttpResponse("something wrong in list_account:")

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
    SA = SwiftAccountMgr()
    result = SA.disable_account(id)
    if result.val:
        return redirect("/accounts/")
    else:
        return HttpResponse("disable_account:"+id)
    
@login_required
def enable_account(request, id):
    """
    """
    SA = SwiftAccountMgr()
    result = SA.enable_account(id)
    if result.val:
        return redirect("/accounts/")
    else:
        return HttpResponse("something wrong in enable_account:"+id)
    
@login_required
def new_account(request):
    """
    """
    print "trackme"
    return render_to_response('new_account.html', { "request": request })

@login_required
def new_account_confirm(request):
    """
    """
    if "account_id" in request.POST:
        account_id = request.POST["account_id"]
        description = request.POST["description"]
        #apply
        SA = SwiftAccountMgr()
        result = SA.add_account(account=account_id, description=description)
        if result.val:
        #    pass
            admin_pw = SA.get_user_password(account=account_id,user="admin")
            if admin_pw.val!=True:
                return HttpResponse("Can't get admin password")
            return render_to_response('confirm_account.html',
              {"account_id":account_id,
               "description":description,
               "identity":"Administrator",
               "request": request,
               "Password":admin_pw.msg})
    else:
        return HttpResponse("new_account_confirm")

#@login_required
#def process_account(request):
#    """
#    """
#    #return HttpResponse("process_account")
#    return redirect("/accounts/")

@login_required
def edit_account(request, id):
    """
    """
    SA = SwiftAccountMgr()
    result = SA.list_user(id)
    if result.val:
        users = SA.list_user(id).msg
        users_list = [];
        for i in users:
            users[i]["id"]=i
            users_list.append(users[i])
        return render_to_response('edit_account.html',
          {"account_id":id, 
           "description":"Long long story......",
           "request": request,
           "users":users_list})
    else:
        return HttpResponse("list user fail in edit_account:"+id)
        
@login_required
def update_account(request, id):
    """
    """
    #TODO: add related api support
    #return HttpResponse("update_account")
    return redirect("/accounts/")

@login_required
def get_password(request, id, user_id):
    """
    """
    SA = SwiftAccountMgr()
    result = SA.get_user_password(account=id, user=user_id)
    if result.val:
        return HttpResponse(result.msg)
    else:
        return HttpResponse("fail to get password")

@login_required
def reset_password(request, id, user_id):
    """
    """
    SA = SwiftAccountMgr()
    stat = SA.change_password(account=id, user=user_id)
    if stat.val:
        result = SA.get_user_password(account=id, user=user_id)
        if result.val:
            return HttpResponse(result.msg)
        else:
            return HttpResponse("fail to get password")
    return HttpResponse("reset_password")

@login_required
def disable_user(request, id, user_id):
    """
    """
    SA = SwiftAccountMgr()
    result = SA.disable_user(id, user_id)
    if result.val:
        return redirect("/accounts/"+id+"/edit")
    else:
        return HttpResponse("disable_user:"+user_id+" in "+id)

@login_required
def enable_user(request, id, user_id):
    """
    """
    SA = SwiftAccountMgr()
    result = SA.enable_user(id, user_id)
    if result.val:
        return redirect("/accounts/"+id+"/edit")
    else:
        return HttpResponse("enable_user:"+user_id+" in "+id)

@login_required
def new_user(request, id):
    """
    """
    #return HttpResponse("new_user")
    return render_to_response('new_user.html', {"account_id":id,
        "request": request })

@login_required
def new_user_confirm(request, id):
    """
    """
    if "user_id" in request.POST:
        user_id = request.POST["user_id"]
        description = request.POST["description"]
        #apply
        SA = SwiftAccountMgr()
        result = SA.add_user(account=id, user=user_id, description=description)
        if result.val:
            user_pw = SA.get_user_password(account=id,user=user_id)
            if user_pw.val!=True:
                return HttpResponse("Can't get "+user_id+" password from "+id)
            return render_to_response('confirm_user.html',
              {"account_id":id,
               "user_id":user_id,
               "description":description,
               "request": request,
               "Password":user_pw.msg})
    else:
        return HttpResponse("new_user_confirm")

#@login_required
#def process_user(request, id):
#    """
#    """
#    return redirect("/accounts/")

@login_required
def edit_user(request, id, user_id):
    """
    """
    #return HttpResponse("edit_user")
    return render_to_response('edit_user.html',
      {"account_id":id, 
       "user_id":user_id,
       "request": request,
       "description":"Long long story......"})

@login_required
def update_user(request, id, user_id):
    """
    """
    #TODO: add related api support
    return HttpResponse("update_user")

#@login_required
#def delete_user(request, id, user_id):
#    """
#    """
#    return HttpResponse("delete_user")
