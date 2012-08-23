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
                current_usage = int(accounts[val]["usage"])
                account_list.append(dict(id=val,usage=current_usage))
        #sort list to get top 5
        top_list = sorted(account_list, key=lambda k: k['usage'])[::-1][:5]
        for i in top_list:
            human_int = i["usage"]/1000000
            human_float = (i["usage"] - (human_int*1000000))/10000
            if(human_float<10):
                append_float="0"+str(human_float)
            else:
                append_float=str(human_float)
            human_read = str(human_int) + "." + append_float +"G"
            i["husage"] = human_read
        return render_to_response('dashboard.html', {"request": request,"zone":zone,"accounts": top_list})
    else:
        return HttpResponse("something wrong in list_account:")
