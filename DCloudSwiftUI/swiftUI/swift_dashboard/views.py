from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse
from DCloudSwift.master.swiftMonitorMgr import SwiftMonitorMgr
#TODO: get account order from model 
from operator import itemgetter
from DCloudSwift.master.swiftAccountMgr import SwiftAccountMgr
from swift_util.helper import human_readable_capacity

@login_required
def index(request):
    """
    dashboard shows top usage accounts, monitor statics, and system error logs
    """
    SM = SwiftMonitorMgr()
    zone = SM.get_zone_info()
    
    #logical maximum capacity
    #total_capacity = 100
    #current usage for all accounts
    #used_capacity = 0
    
    SA = SwiftAccountMgr()
    result = SA.list_account()
    if result.val:
        accounts = result.msg
        account_list = []
        for val in accounts:
            if(accounts[val]["account_enable"] and accounts[val]["usage"]!="Error"):
                current_usage = int(accounts[val]["usage"])
                #current_quota = int(accounts[val]["quota"])
                #used_capacity+=current_usage
                #total_capacity+=current_quota
                account_list.append(dict(id=val,usage=current_usage))
        #sort list to get top 5
        top_list = sorted(account_list, key=itemgetter('usage'))[::-1][:5]
        for i in top_list:
            i["husage"] = human_readable_capacity(i["usage"])
        #used_ratio = (used_capacity*100)/total_capacity
        #zone["used"] = used_ratio
        #zone["free"] = 100-used_ratio
        #zone["quota"] = human_readable_capacity(total_capacity)
        return render_to_response('dashboard.html', {"request": request,"zone":zone,"accounts": top_list})
    else:
        return HttpResponse(result.msg)
