from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse
from DCloudSwift.master.swiftMonitorMgr import SwiftMonitorMgr

@login_required
def index(request):
    """
    monitor swift nodes
    """
    zone = {"ip":"192.168.1.104","nodes":3,"used":"21","free":"79","capacity":"12TB"}
    SM = SwiftMonitorMgr()
    zone = SM.get_zone_info()
    nodes = SM.list_nodes_info()

    return render_to_response('monitor.html', {"request": request, "zone":zone,"nodes":nodes})
