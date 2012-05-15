from django.shortcuts import render
from django.contrib.auth.decorators import login_required
from django import forms
from lib.forms import RenderFormMixinClass
from lib.forms import IPAddressInput, NumberInput
from lib.gateway import api
import json

@login_required
def index(request):
    return render(request, 'dashboard/dashboard.html')


def system(request):

    class Network(RenderFormMixinClass, forms.Form):
        ip = forms.IPAddressField(label='IP Address', widget=IPAddressInput)
        gateway = forms.IPAddressField(label='Gateway Address', widget=IPAddressInput)
        mask = forms.IPAddressField(label='Submask', widget=IPAddressInput)
        dns1 = forms.IPAddressField(label='Primary DNS server', widget=IPAddressInput)
        dns2 = forms.IPAddressField(label='Secondary DNS server', widget=IPAddressInput)

    class AdminPassword(RenderFormMixinClass, forms.Form):
        password = forms.CharField(widget=forms.PasswordInput)
        retype_password = forms.CharField(widget=forms.PasswordInput)

    if request.method == "POST":
        pass
    else:
        network_data = json.loads(api.get_network()).get('data')
        forms_group = [Network(**network_data), AdminPassword()]

    return render(request, 'dashboard/form_tab.html', {'tab': 'system', 'forms_group': forms_group})


@login_required
def account(request):

    class Gateway(RenderFormMixinClass, forms.Form):
        username = forms.CharField()
        password = forms.CharField(widget=forms.PasswordInput)
        verify_password = forms.CharField(widget=forms.PasswordInput)
        retype_password = forms.CharField(widget=forms.PasswordInput)

    class EncryptionKey(RenderFormMixinClass, forms.Form):
        ip_address = forms.IPAddressField(label='IP Address', widget=IPAddressInput)
        port = forms.IntegerField(widget=NumberInput)
        username = forms.CharField()
        password = forms.CharField(widget=forms.PasswordInput)

    forms_group = [Gateway(), EncryptionKey()]

    return render(request, 'dashboard/form_tab.html', {'tab': 'account', 'forms_group': forms_group})


@login_required
def sharefolder(request):
    return render(request, 'dashboard/sharefolder.html', {'tab': 'sharefolder'})


@login_required
def sync(request):
    return render(request, 'dashboard/sync.html', {'tab': 'sync'})


@login_required
def syslog(request):
    return render(request, 'dashboard/syslog.html', {'tab': 'syslog'})


@login_required
def power(request):
    return render(request, 'dashboard/power.html', {'tab': 'power'})
