from django.shortcuts import render, HttpResponse
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
    network_data = json.loads(api.get_network()).get('data')
    print network_data

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
            form = AdminPassword(request.POST)
            if form.is_valid():
                request.user.set_password(form['password'].data)
                request.user.save()
            else:
                forms_group[action] = form

    forms_group['network'] = forms_group.get('network', Network(initial=network_data))
    forms_group['admin_pass'] = forms_group.get('admin_pass', AdminPassword())
    return render(request, 'dashboard/form_tab.html', {'tab': 'system', 'forms_group': forms_group})


@login_required
def account(request, action=None):

    forms_group = {}
    gateway_data = json.loads(api.get_storage_account()).get('data')
    print gateway_data

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
def sharefolder(request, action):

    forms_group = {}
    smb_data = json.loads(api.get_smb_user_list()).get('data')
    smb_data['username'] = smb_data['accounts'][0]
    print smb_data
    nfs_ip_query = json.loads(api.get_nfs_access_ip_list())
    if nfs_ip_query['result']:
        nfs_data = nfs_ip_query.data
    else:
        nfs_data = {'array_of_ip': ['192.168.1.1', '192.168.1.1']}

    class SMBSetting(RenderFormMixinClass, forms.Form):
        username = forms.CharField()
        password = forms.CharField(widget=forms.PasswordInput)

    class NFSSetting(RenderFormMixinClass, forms.Form):
        array_of_ip = forms.MultipleChoiceField(choices=tuple([(ip, ip) for ip in nfs_data['array_of_ip']]))

    if request.method == "POST":
        if action == "smb_setting":
            form = SMBSetting(request.POST)
            if form.is_valid():
                update_return = json.loads(api.set_smb_user_list(**form.cleaned_data))
                if not update_return['result']:
                    print update_return['msg']
        elif action == "nfs_setting":
            print request.POST

    forms_group['smb_setting'] = forms_group.get('smb_setting', SMBSetting(initial=smb_data))
    forms_group['nfs_setting'] = forms_group.get('nfs_setting', NFSSetting(initial=nfs_data))

    return render(request, 'dashboard/form_tab.html', {'tab': 'sharefolder', 'forms_group': forms_group})


@login_required
def sync(request):
    if request.method == "POST":
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


@login_required
def indicator(request):
    indicator_data = json.loads(api.get_gateway_indicators())
    if indicator_data['result']:
        result_data = json.dumps(indicator_data['data'])
        return HttpResponse(result_data)
    else:
        return HttpResponse(indicator_data['msg'], status=500)


@login_required
def gateway_status(request):
    try:
        data = json.loads(api.get_gateway_status())
    except:
        data = {}
    data['cache_usage'] = {"max_cache_size": 1000,
                           "max_cache_entries": 100,
                           "used_cache_size": 500,
                           "used_cache_entries": 50,
                           "dirty_cache_size": 100,
                           "dirty_cache_entries": 10}
    return HttpResponse(json.dumps(data['cache_usage']))
