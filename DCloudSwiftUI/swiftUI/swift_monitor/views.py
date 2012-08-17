from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse

@login_required
def index(request):
    """
    monitor swift nodes
    """
    zone = {"ip":"192.168.1.104","nodes":3,"used":"21","free":"79","capacity":"12TB"}
    nodes = []
    nodes.append({"ip":"172.30.11.33","hostname":"TPEIIA","status":"dead","mode":"waiting","hd_number":6,"hd_error":1,
    "hd_info":[{"serial":"SN_TP02","status":"Broken"},{"serial":"SN_TP03","status":"OK"},{"serial":"SN_TP04","status":"OK"}]
    })
    nodes.append({"ip":"172.30.11.37","hostname":"TPEIIB","status":"alive","mode":"waiting","hd_number":6,"hd_error":0,
    "hd_info":[{"serial":"SN_TP02","status":"OK"},{"serial":"SN_TP03","status":"OK"},{"serial":"SN_TP04","status":"OK"}]
    })
    nodes.append({"ip":"172.30.11.25","hostname":"TPEIIC","status":"alive","mode":"service","hd_number":6,"hd_error":0,
    "hd_info":[{"serial":"SN_TP02","status":"OK"},{"serial":"SN_TP03","status":"OK"},{"serial":"SN_TP04","status":"OK"}]
    })
    return render_to_response('monitor.html', {"request": request, "zone":zone,"nodes":nodes})