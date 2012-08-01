from django.shortcuts import render_to_response
from django.http import HttpResponse

def index(request):
    """
    ajax call servtime every minute
    """
    return render_to_response('template/index.html', {})

def server_time(request):
    """
    ajax call
    """
    #return server time
    return HttpResponse("server time")