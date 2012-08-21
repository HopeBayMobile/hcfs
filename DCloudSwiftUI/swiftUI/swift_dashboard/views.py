from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse

@login_required
def index(request):
    """
    dashboard
    """
    zone = {"ip":"192.168.1.104","nodes":3,"used":"21","free":"79","capacity":"12TB"}
    
    return render_to_response('dashboard.html', {"request": request,"zone":zone})