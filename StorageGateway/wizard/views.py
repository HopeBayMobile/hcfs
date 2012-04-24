from django.shortcuts import render, redirect
from django.contrib.auth.decorators import login_required
from django import forms

from lib.models.config import Config
from lib.forms import RenderFormMixinClass
from lib.forms.widgets import *

class InstallationFrom(RenderFormMixinClass, forms.Form):
    RAID_CHOICES = [
        ('', ''),
        ('raid-0', 'RAID 0'),
        ('raid-1', 'RAID 1'),
    ]
    
    #Network
    ip_address = forms.IPAddressField(label='IP address', initial='192.168.0.1')
    subnet_mask = forms.IPAddressField(initial='255.255.255.0')
    #Cloud Storage
    cloud_username = forms.CharField(label='Username of cloud')
    cloud_password = forms.CharField(label='Passward of cloud')
    #Security
    encryption_key = forms.CharField()
    #Disk
    raid = forms.ChoiceField(label="RAID", choices=RAID_CHOICES)
    #Share Folder
    shared_folder_name = forms.CharField(initial='SaveBox')
    folder_username = forms.CharField(label='Username for folder', initial='savebox')
    folder_password = forms.CharField(label='Passward for folder', initial='savebox')

def index(request):
    if request.method == 'POST':
        form = InstallationFrom(request.POST)
        if form.is_valid():
            #Call storage gateway API
            
            
            
            #Save settings into lib_config
            for field in form.fields:
                c = Config(key=field, value=form.data[field])
                c.save()
            
            return render(request, 'home.html')            
    else:
        form = InstallationFrom()
    
    return render(request, 'form.html', {'header': 'System Installation',
                                         'form': form,
                                         'action': '.',
                                         })

