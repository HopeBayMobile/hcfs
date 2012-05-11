from django.shortcuts import render
from django.contrib.auth.decorators import login_required

@login_required
def index(request):
    return render(request, 'dashboard/container.html')

@login_required
def account_configuration(request):
    return render(request, 'account_configuration.html')

@login_required
def sync_management(request):
    return render(request, 'sync_management.html')

@login_required
def system_log(request):
    return render(request, 'system_log.html')
