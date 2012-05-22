from django.shortcuts import render, HttpResponse
from django.contrib.auth.decorators import login_required
from django import forms
from django.utils.datastructures import SortedDict
from lib.forms import RenderFormMixinClass
from lib.forms import IPAddressInput
from gateway import api
import json


def gateway_status():
    """
    This function retrive all gateway status from api and return a mock data
    if the query is failed.
    """
    try:
        data = json.loads(api.get_gateway_status())
        print data
    except Exception as inst:
        print inst
        data = {}
    data['cache_usage'] = {"max_cache_size": 1000,
                           "max_cache_entries": 100,
                           "used_cache_size": 500,
                           "used_cache_entries": 50,
                           "dirty_cache_size": 100,
                           "dirty_cache_entries": 10}
    return data


@login_required
def index(request):
    data = gateway_status()
    cache_usage = data['cache_usage']
    maxcache = cache_usage["max_cache_size"]
    context = {"cache_usage": cache_usage,
               "dirty_cache_percentage": cache_usage['dirty_cache_size'] * 100 / maxcache,
               "used_cache_percentage": cache_usage['used_cache_size'] * 100 / maxcache}
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
                    raise forms.ValidationError("The password are different!")
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
                print update_return
                if not update_return['result']:
                    action_error[action] = update_return['msg']
            else:
                forms_group[action] = form
        elif action == "encrypt":
            form = EncryptionKey(request.POST)
            if form.is_valid():
                update_return = json.loads(api.apply_user_enc_key(form['password'].data))
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
    smb_data = json.loads(api.get_smb_user_list()).get('data')
    action_error = {}
    smb_data['username'] = smb_data['accounts'][0]
    print smb_data
    nfs_ip_query = json.loads(api.get_nfs_access_ip_list())
    if nfs_ip_query['result']:
        nfs_data = nfs_ip_query['data']
    else:
        nfs_data = {'array_of_ip': ['8.8.8.8']}

    class SMBSetting(RenderFormMixinClass, forms.Form):
        username = forms.CharField()
        password = forms.CharField(widget=forms.PasswordInput)

    def nfs_setting_generator(ip_list):
        class NFSSetting(RenderFormMixinClass, forms.Form):
            array_of_ip = forms.MultipleChoiceField(choices=tuple([(ip, ip) for ip in ip_list]))
        return NFSSetting

    if request.method == "POST":
        if action == "smb_setting":
            form = SMBSetting(request.POST)
            if form.is_valid():
                update_return = json.loads(api.set_smb_user_list(**form.cleaned_data))
                print update_return
                if not update_return['result']:
                    action_error[action] = update_return['msg']
        elif action == "nfs_setting":
            ip_list = request.POST.getlist('array_of_ip')
            update_return = json.loads(api.set_nfs_access_ip_list(ip_list))
            print update_return
            if not update_return['result']:
                action_error[action] = update_return['msg']
                form = nfs_setting_generator(set(nfs_data['array_of_ip'] + ip_list))
                forms_group['nfs_setting'] = form

    forms_group['smb_setting'] = forms_group.get('smb_setting', SMBSetting(initial=smb_data))
    forms_group['nfs_setting'] = forms_group.get('nfs_setting', nfs_setting_generator(nfs_data['array_of_ip'])())

    return render(request, 'dashboard/form_tab.html', {'tab': 'sharefolder', 'forms_group': forms_group, 'action_error': action_error})


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
                    array = [day, 0, 24, 0]
                else:
                    array = [day, interval_from, interval_to, bandwidth]
                data[now] = array

        api.apply_scheduling_rules(data)

    hours = range(0, 24)
    weeks_data = SortedDict()
    weeks_data[1] = {'name': 'Mon.', 'range': 'all', 'bandwidth': '0'}
    weeks_data[2] = {'name': 'Thu.', 'range': 'all', 'bandwidth': '0'}
    weeks_data[3] = {'name': 'Wed.', 'range': 'all', 'bandwidth': '0'}
    weeks_data[4] = {'name': 'Thu.', 'range': 'all', 'bandwidth': '0'}
    weeks_data[5] = {'name': 'Fri.', 'range': 'all', 'bandwidth': '0'}
    weeks_data[6] = {'name': 'Sat.', 'range': 'all', 'bandwidth': '0'}
    weeks_data[7] = {'name': 'Sun.', 'range': 'all', 'bandwidth': '0'}

    for value in data:
        day = int(value[0])
        rstart = int(value[1])
        rend = int(value[2])
        upload_limit = int(value[3])

        if upload_limit == 0 and rstart == 0 and rend == 24:
            weeks_data[day]['range'] = "all"
        elif upload_limit < 0:
            weeks_data[day]['range'] = "none"
        else:
            weeks_data[day]['range'] = range(rstart, rend)

        weeks_data[day]['bandwidth'] = upload_limit

    return render(request, 'dashboard/sync.html', {'tab': 'sync', 'hours':
        hours, 'weeks_data': weeks_data})


fake_syslog = {
    "msg": "GOGOGO",
    "data": {
        "error_log": [
            {
                "category": "gateway",
                "timestamp": "2012-05-01",
                "msg": "stringstringstringstringstringstringstringstringstringstring"
            },
            {
                "category": "NFS",
                "timestamp": "2012-05-04",
                "msg": "stringstringstringstringstringstringstringstringstringstring"
            },
            {
                "category": "SMB",
                "timestamp": "2012-05-07",
                "msg": "stringstringstringstringstringstringstringstringstringstring"
            }
        ],
        "warning_log": [
            {
                "category": "gateway",
                "timestamp": "2012-05-02",
                "msg": "stringstringstringstringstringstringstringstringstringstring"
            },
            {
                "category": "NFS",
                "timestamp": "2012-05-05",
                "msg": "stringstringstringstringstringstringstringstringstringstring"
            },
            {
                "category": "SMB",
                "timestamp": "2012-05-08",
                "msg": "stringstringstringstringstringstringstringstringstringstring"
            }
        ],
        "info_log": [
            {
                "category": "gateway",
                "timestamp": "2012-05-03",
                "msg": "stringstringstringstringstringstringstringstringstringstring"
            },
            {
                "category": "NFS",
                "timestamp": "2012-05-06",
                "msg": "stringstringstringstringstringstringstringstringstringstring"
            },
            {
                "category": "SMB",
                "timestamp": "2012-05-09",
                "msg": "stringstringstringstringstringstringstringstringstringstring"
            }
        ]
    },
    "result": True
}


@login_required
def syslog(request):
    return_val = fake_syslog
    # FIXME: Replace the fake logs
    # log_data = json.loads(api.get_gateway_system_log(0, 100, 'gateway'))
    if return_val['result'] == True:
        log_data = return_val['data']

    return render(request, 'dashboard/syslog.html', {'tab': 'syslog',
                                                     'log_data': log_data
                                                     })


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
def get_syslog(request, category, level):
    log_data = json.loads(api.get_gateway_system_log(int(level), 100, category))
    return HttpResponse(json.dumps(log_data['data']))


@login_required
def gateway_cache_usage(request):
    data = gateway_status()
    return HttpResponse(json.dumps(data['cache_usage']))
