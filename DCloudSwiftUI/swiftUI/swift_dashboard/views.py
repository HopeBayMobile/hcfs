from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse
from DCloudSwift.master.swiftMonitorMgr import SwiftMonitorMgr

@login_required
def index(request):
    """
    dashboard
    """
    SM = SwiftMonitorMgr()
    zone = SM.get_zone_info()
    return render_to_response('dashboard.html', {"request": request,"zone":zone})