from django.shortcuts import render, redirect
from django.contrib.auth.decorators import login_required
from django import forms

from lib.forms import RenderFormMixinClass
from lib.forms.widgets import *


class InstallationFrom(RenderFormMixinClass, forms.Form):
    #Network
    ip_address = forms.IPAddressField(label='IP address')
    subnet_mask = forms.IPAddressField()
    #Cloud Storage
    cloud_username = forms.CharField(label='Username of cloud')
    cloud_password = forms.CharField(label='Passward of cloud')
    #Security
    encryption_key = forms.CharField()
    #Disk
    raid = forms.CharField(label="RAID")
    #Share Folder
    shared_folder = forms.CharField()
    folder_username = forms.CharField(label='Username for folder')
    folder_password = forms.CharField(label='Passward for folder')

def index(request):   
    return render(request, 'form.html', {'header': 'System Installation',
                                         'form': InstallationFrom(),
                                         'action': 'create'
                                         })
def create(request):
    #Validation

    
    #Save settings into lib_config
    
    
    return render(request, 'home.html')
