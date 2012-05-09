from django.shortcuts import render
from django.contrib.auth.decorators import login_required

@login_required
def index(request):
    return render(request, 'dashboard.html')

@login_required
def account_configuration(request):
    return render(request, 'account_configuration.html')
