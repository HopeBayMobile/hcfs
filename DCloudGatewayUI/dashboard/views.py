from django.shortcuts import render
from django.contrib.auth.decorators import login_required
from django import forms
from django.utils.datastructures import SortedDict
from lib.forms import RenderFormMixinClass
from lib.forms import IPAddressInput
from gateway import api
import json


@login_required
def index(request):
    return render(request, 'dashboard/dashboard.html')


def system(request, action=None):

    forms_group = {}
    network_data = {}

    class Network(RenderFormMixinClass, forms.Form):
        ip = forms.IPAddressField(label='IP Address', widget=IPAddressInput)
        gateway = forms.IPAddressField(label='Gateway Address', widget=IPAddressInput)
        mask = forms.IPAddressField(label='Submask', widget=IPAddressInput)
        dns1 = forms.IPAddressField(label='Primary DNS server', widget=IPAddressInput)
        dns2 = forms.IPAddressField(label='Secondary DNS server', widget=IPAddressInput)

    class AdminPassword(RenderFormMixinClass, forms.Form):
        password = forms.CharField(widget=forms.PasswordInput)
        retype_password = forms.CharField(widget=forms.PasswordInput)

        def clean(self):
            cleaned_data = super(AdminPassword, self).clean()
            new_passord = cleaned_data.get('password')
            if new_passord:
                if new_passord == cleaned_data['retype_password']:
                    del cleaned_data['retype_password']
                else:
                    raise forms.ValidationError("The password are different!")
            return cleaned_data

    if request.method == "POST":
        if action == "network":
            form = Network(request.POST)
            if form.is_valid():
                update_return = json.loads(api.apply_network(**form.cleaned_data))
                if not update_return['result']:
                    print update_return['msg']
            else:
                forms_group[action] = form
        elif action == "admin_pass":
            network_data = json.loads(api.get_network()).get('data')
            form = AdminPassword(request.POST)
            if form.is_valid():
                request.user.set_password(form['password'].data)
            else:
                forms_group[action] = form

    forms_group['network'] = forms_group.get('network', Network(initial=network_data))
    forms_group['admin_pass'] = forms_group.get('admin_pass', AdminPassword())
    return render(request, 'dashboard/form_tab.html', {'tab': 'system', 'forms_group': forms_group})


@login_required
def account(request, action=None):

    forms_group = {}
    gateway_data = {}

    class Gateway(RenderFormMixinClass, forms.Form):
        storage_url = forms.CharField()
        account = forms.CharField()
        password = forms.CharField(widget=forms.PasswordInput)
        retype_password = forms.CharField(widget=forms.PasswordInput)

        def clean(self):
            cleaned_data = super(Gateway, self).clean()
            new_passord = cleaned_data.get('password')
            if new_passord:
                if new_passord == cleaned_data['retype_password']:
                    del cleaned_data['retype_password']
                else:
                    raise forms.ValidationError("The password are different!")
            return cleaned_data

    class EncryptionKey(RenderFormMixinClass, forms.Form):
        password = forms.CharField(widget=forms.PasswordInput, min_length=6, max_length=20)
        retype_password = forms.CharField(widget=forms.PasswordInput, min_length=6, max_length=20)

        def clean(self):
            cleaned_data = super(EncryptionKey, self).clean()
            new_passord = cleaned_data.get('password')
            if new_passord:
                if new_passord == cleaned_data['retype_password']:
                    del cleaned_data['retype_password']
                else:
                    raise forms.ValidationError("The password are different!")
            return cleaned_data

    if request.method == "POST":
        if action == "gateway":
            form = Gateway(request.POST)
            if form.is_valid():
                update_return = json.loads(api.apply_storage_account(**form.cleaned_data))
                if not update_return['result']:
                    print update_return['msg']
            else:
                forms_group[action] = form
        elif action == "encrypt":
            gateway_data = json.loads(api.get_storage_account()).get('data')
            form = EncryptionKey(request.POST)
            if form.is_valid():
                update_return = json.loads(api.apply_user_enc_key(form['password'].data))
                if not update_return['result']:
                    print update_return['msg']
            else:
                forms_group[action] = form

    forms_group['gateway'] = forms_group.get('gateway', Gateway(initial=gateway_data))
    forms_group['encrypt'] = forms_group.get('encrypt', EncryptionKey())

    return render(request, 'dashboard/form_tab.html', {'tab': 'account', 'forms_group': forms_group})


@login_required
def sharefolder(request):
    return render(request, 'dashboard/sharefolder.html', {'tab': 'sharefolder'})


@login_required
def sync(request):
    if request.method == "POST" :
        print request.POST

    hours = range(0, 24)
    weeks_data = SortedDict()
    weeks_data['Mon.'] = "all"
    weeks_data['Tue.'] = "all"
    weeks_data['Wed.'] = "all"
    weeks_data['Thu.'] = "all"
    weeks_data['Fri.'] = "all"
    weeks_data['Sat.'] = "none"
    weeks_data['Sun.'] = range(6, 18)

    return render(request, 'dashboard/sync.html', {'tab': 'sync', 'hours':
        hours, 'weeks_data': weeks_data})


@login_required
def syslog(request):
    return render(request, 'dashboard/syslog.html', {'tab': 'syslog'})


@login_required
def power(request):
    return render(request, 'dashboard/power.html', {'tab': 'power'})
