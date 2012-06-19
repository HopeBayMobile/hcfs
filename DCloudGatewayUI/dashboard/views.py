from django.shortcuts import render, HttpResponse, redirect
from django.contrib.auth.decorators import login_required
from django import forms
from django.conf import settings
from django.views.decorators.http import require_POST
from django.utils.datastructures import SortedDict
from lib.forms import RenderFormMixinClass
from lib.forms import IPAddressInput
import json


if not getattr(settings, "DEBUG", False):
    from gateway import api
    from gateway import api_restore_conf
    from gateway import api_remote_upgrade
    from http_proxy import api_http_proxy
else:
    from gateway.mock import api
    from gateway.mock import api_restore_conf
    from gateway.mock import api_remote_upgrade
    from http_proxy.mock import api_http_proxy


def gateway_status():
    """
    This function retrive all gateway status from api and return a mock data
    if the query is failed.
    """
    try:
        data = json.loads(api.get_gateway_status())['data']
    except Exception as inst:
        print inst
        data = {}
    return data


@login_required
def index(request):
    data = gateway_status()
    cache_usage = data['gateway_cache_usage']
    maxcache = cache_usage["max_cache_size"] or 1
    context = {"dirty_cache_percentage": cache_usage['dirty_cache_size'] * 100 / maxcache,
               "used_cache_percentage": cache_usage['used_cache_size'] * 100 / maxcache}
    context.update(data)
    try:
        version = {"current_version": json.loads(api_remote_upgrade.get_gateway_version()).get("version"),
                   "available_version": json.loads(api_remote_upgrade.get_available_upgrade()).get("version")}
    except Exception as inst:
        print inst
    context.update(version)
    return render(request, 'dashboard/dashboard.html', context)


def system(request, action=None):

    forms_group = {}
    network_data = json.loads(api.get_network()).get('data')
    action_error = {}
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
                    raise forms.ValidationError("The password does not match the retype password!")
            return cleaned_data

    if request.method == "POST":
        if action == "network":
            form = Network(request.POST)
            if form.is_valid():
                update_return = json.loads(api.apply_network(**form.cleaned_data))
                print update_return
                if not update_return['result']:
                    action_error[action] = update_return['msg']
            else:
                forms_group[action] = form
        elif action == "admin_pass":
            form = AdminPassword(request.POST)
            if form.is_valid():
                request.user.set_password(form['password'].data)
                request.user.save()
                return redirect('/logout')
            else:
                forms_group[action] = form

    forms_group['network'] = forms_group.get('network', Network(initial=network_data))
    forms_group['admin_pass'] = forms_group.get('admin_pass', AdminPassword())
    return render(request, 'dashboard/form_tab.html', {'tab': 'system', 'forms_group': forms_group, 'action_error': action_error})


@login_required
def account(request, action=None):

    forms_group = {}
    gateway_data = json.loads(api.get_storage_account()).get('data')
    action_error = {}
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
                    raise forms.ValidationError("The password does not match the retype password!")
            return cleaned_data

    class EncryptionKey(RenderFormMixinClass, forms.Form):
        old_key = forms.CharField(widget=forms.PasswordInput, min_length=6, max_length=20)
        new_key = forms.CharField(widget=forms.PasswordInput, min_length=6, max_length=20)
        retype_new_key = forms.CharField(widget=forms.PasswordInput, min_length=6, max_length=20)

        def clean(self):
            cleaned_data = super(EncryptionKey, self).clean()
            new_key = cleaned_data.get('new_key')
            if new_key:
                if new_key == cleaned_data['retype_new_key']:
                    del cleaned_data['retype_new_key']
                else:
                    raise forms.ValidationError("The newkey does not match the retype key!")
            return cleaned_data

    if request.method == "POST":
        if action == "gateway":
            form = Gateway(request.POST)
            if form.is_valid():
                update_return = json.loads(api.apply_storage_account(**form.cleaned_data))
                print update_return
                if not update_return['result']:
                    action_error[action] = update_return['msg']
            else:
                forms_group[action] = form
        elif action == "encrypt":
            form = EncryptionKey(request.POST)
            if form.is_valid():
                update_return = json.loads(api.apply_user_enc_key(form['old_key'].data, form['new_key'].data))
                print update_return
                if not update_return['result']:
                    action_error[action] = update_return['msg']
            else:
                forms_group[action] = form

    forms_group['gateway'] = forms_group.get('gateway', Gateway(initial=gateway_data))
    forms_group['encrypt'] = forms_group.get('encrypt', EncryptionKey())

    return render(request, 'dashboard/form_tab.html', {'tab': 'account', 'forms_group': forms_group, 'action_error': action_error})


@login_required
def sharefolder(request, action):

    forms_group = {}
    action_error = {}
    smb_data = {"username": json.loads(api.get_smb_user_list()).get('data').get('username')}
    nfs_ip_query = json.loads(api.get_nfs_access_ip_list())
    if nfs_ip_query['result']:
        nfs_data = nfs_ip_query['data']
    else:
        nfs_data = {'array_of_ip': ['8.8.8.8']}

    class SMBSetting(RenderFormMixinClass, forms.Form):
        username = forms.CharField()
        password = forms.CharField(widget=forms.PasswordInput)

    if request.method == "POST":
        if action == "smb_setting":
            form = SMBSetting(request.POST)
            if form.is_valid():
                update_return = json.loads(api.set_smb_user_list(**form.cleaned_data))
                print update_return
                if not update_return['result']:
                    action_error[action] = update_return['msg']
        elif action == "nfs_setting":
            ip_list = json.loads(request.POST.get('array_of_ip'))
            update_return = json.loads(api.set_nfs_access_ip_list(ip_list))
            print update_return
            if not update_return['result']:
                action_error[action] = update_return['msg']
            nfs_data['array_of_ip'] = ip_list

    forms_group['smb_setting'] = forms_group.get('smb_setting', SMBSetting(initial=smb_data))

    return render(request, 'dashboard/sharefolder.html', {'tab': 'sharefolder', 'forms_group': forms_group,
                                                          'action_error': action_error, "nfs_data": nfs_data})


@login_required
def sync(request):
    load_data = json.loads(api.get_scheduling_rules())
    data = load_data['data']

    if request.method == "POST":
        query = request.POST
        day = query['day']
        bandwidth_option = query['bandwidth_option']
        interval_to = query['interval_to']
        interval_from = query['interval_from']
        bandwidth = query['bandwidth']

        for value in data:
            if value[0] == day:
                now = int(value[0]) - 1
                array = []
                if bandwidth_option == "1":
                    array = [day, 0, 24, -1]
                elif bandwidth_option == "2":
                    array = [day, 0, 24, bandwidth]
                else:
                    array = [day, interval_from, interval_to, bandwidth]
                data[now] = array

        api.apply_scheduling_rules(data)

    hours = range(0, 24)
    weeks_data = SortedDict()

    for i in range(1, 8):
        weeks_data[i] = {'name': '',
                         'range': 'all',
                         'bandwidth': '0',
                         'interval_from': '-1',
                         'interval_to': '-1',
                         'option': 1}

    weeks_data[1]['name'] = 'Mon.'
    weeks_data[2]['name'] = 'Tue.'
    weeks_data[3]['name'] = 'Wed.'
    weeks_data[4]['name'] = 'Thu.'
    weeks_data[5]['name'] = 'Fri.'
    weeks_data[6]['name'] = 'Sat.'
    weeks_data[7]['name'] = 'Sun.'

    for value in data:
        day = int(value[0])
        rstart = int(value[1])
        rend = int(value[2])
        upload_limit = int(value[3])

        if upload_limit > 0 and rstart == 0 and rend == 24:
            weeks_data[day]['range'] = "all"
            weeks_data[day]['option'] = 2
        elif upload_limit < 0:
            weeks_data[day]['range'] = "none"
            weeks_data[day]['option'] = 1
        else:
            weeks_data[day]['range'] = range(rstart, rend + 1)
            weeks_data[day]['option'] = 3
            weeks_data[day]['interval_from'] = rstart
            weeks_data[day]['interval_to'] = rend

        weeks_data[day]['bandwidth'] = upload_limit

    return render(request, 'dashboard/sync.html', {'tab': 'sync', 'hours':
        hours, 'weeks_data': weeks_data})


@login_required
def syslog(request):
    return_val = json.loads(api.get_gateway_system_log(0, 100, 'gateway'))
    if return_val['result'] == True:
        log_data = return_val['data']

    if request.is_ajax():
        return HttpResponse(json.dumps(log_data['error_log']))

    return render(request, 'dashboard/syslog.html', {'tab': 'syslog',
                                                     'log_data': log_data
                                                     })


@login_required
@require_POST
def http_proxy(request, action=None):
    if request.method == 'POST':
        result = json.loads(api_http_proxy.set_http_proxy(action))
        return HttpResponse(result)


@login_required
@require_POST
def system_upgrade(request):
    try:
        api_remote_upgrade.upgrade_gateway()
        return HttpResponse("Upgrade Success.")
    except:
        return HttpResponse("Upgrade Failed.")


@login_required
def power(request, action=None):
    if request.method == 'POST':
        if action == 'poweroff':
            result = json.loads(api.shutdown_gateway())
        elif action == 'restart':
            result = json.loads(api.reset_gateway())
        return HttpResponse(result)

    return render(request, 'dashboard/power.html', {'tab': 'power'})


@login_required
def indicator(request):
    indicator_data = json.loads(api.get_gateway_indicators())
    if indicator_data['result']:
        result_data = json.dumps(indicator_data['data'])
        return HttpResponse(result_data)
    else:
        return HttpResponse(indicator_data['msg'], status=500)


def status(request):
    status_data = gateway_status()
    return HttpResponse(json.dumps(status_data))


@login_required
def cache_usage(request):
    data = gateway_status()
    return HttpResponse(json.dumps(data['gateway_cache_usage']))


@login_required
def config(request, action=None):
    if request.method == 'POST':
        info = '{"result":false}'
        if action == 'restore':
            info = api_restore_conf.restore_gateway_configuration()
        elif action == 'save':
            info = api_restore_conf.save_gateway_configuration()
        result = json.loads(info)
        return HttpResponse(result)

    info = api_restore_conf.get_configuration_backup_info()
    backup_info = json.loads(info)
    backup_time = backup_info['backup_time']

    return render(request, 'dashboard/config.html', {'backup_time': backup_time})
