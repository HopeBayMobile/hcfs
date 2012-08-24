from django.shortcuts import render_to_response, redirect
from django.contrib.auth.decorators import login_required
from django.http import HttpResponse

@login_required
def index(request):
    """
    show system logs
    """
    return render_to_response('syslog.html', {"request": request})
    