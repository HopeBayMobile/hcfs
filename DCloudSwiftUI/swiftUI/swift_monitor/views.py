from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse
from DCloudSwift.master.swiftMonitorMgr import SwiftMonitorMgr

@login_required
def index(request):
    """
    monitor swift nodes
    """
    SM = SwiftMonitorMgr()
    zone = SM.get_zone_info()
    nodes = SM.list_nodes_info()
    return render_to_response('monitor.html', {"request": request, "zone":zone,"nodes":nodes})