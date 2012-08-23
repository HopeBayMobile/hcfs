from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse
from DCloudSwift.master.swiftMonitorMgr import SwiftMonitorMgr
#TODO: get account order from model 
from DCloudSwift.master.swiftAccountMgr import SwiftAccountMgr

@login_required
def index(request):
    """
    dashboard
    """
    SM = SwiftMonitorMgr()
    zone = SM.get_zone_info()
    
    SA = SwiftAccountMgr()
    result = SA.list_account()
    if result.val:
        accounts = result.msg
        account_list = []
        for val in accounts:
            if(accounts[val]["account_enable"]):
                current_quota = int(accounts[val]["quota"])
                account_list.append(dict(id=val,quota=current_quota))
        #sort list to get top 5
        top_list = sorted(account_list, key=lambda k: k['quota'])[::-1][:5]
        return render_to_response('dashboard.html', {"request": request,"zone":zone,"accounts": top_list})
    else:
        return HttpResponse("something wrong in list_account:")
