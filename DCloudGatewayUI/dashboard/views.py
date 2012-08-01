from django.shortcuts import render, HttpResponse, redirect
from django.contrib.auth.decorators import login_required
from django import forms
from django.conf import settings
from django.views.decorators.http import require_POST
from django.utils.datastructures import SortedDict
from lib.forms import RenderFormMixinClass
from lib.forms import IPAddressInput
import json
import datetime
import time


if not getattr(settings, "DEBUG", False):
    from gateway import api
    from gateway import api_restore_conf
    from gateway import api_remote_upgrade
    from gateway import snapshot as api_snapshot
    from http_proxy import api_http_proxy
else:
    from gateway.mock import api
    from gateway.mock import api_restore_conf
    from gateway.mock import api_remote_upgrade
    from gateway.mock import snapshot as api_snapshot
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
        context["available_version"] = json.loads(api_remote_upgrade.get_available_upgrade()).get("version")
    except Exception as inst:
        print inst
    return render(request, 'dashboard/dashboard.html', context)


def system(request, action=None):

    forms_group = SortedDict()
    network_data = json.loads(api.get_network()).get('data')
    gateway_data = json.loads(api.get_storage_account()).get('data')
    action_error = {}
    print network_data

    class Gateway(RenderFormMixinClass, forms.Form):
        storage_url = forms.CharField(label='Cloud storage server')
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

    class Network(RenderFormMixinClass, forms.Form):
        ip = forms.IPAddressField(label='IP address', widget=IPAddressInput)
        mask = forms.IPAddressField(label='Submask', widget=IPAddressInput)
        gateway = forms.IPAddressField(label='Default gateway', widget=IPAddressInput)
        dns1 = forms.IPAddressField(label='Primary DNS server', widget=IPAddressInput)
        dns2 = forms.IPAddressField(label='Secondary DNS server', widget=IPAddressInput)

    class AdminPassword(RenderFormMixinClass, forms.Form):
        password = forms.CharField(label='New password', widget=forms.PasswordInput)
        retype_password = forms.CharField('Retype new password', widget=forms.PasswordInput)

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
        if action == "Network":
            form = Network(request.POST)
            if form.is_valid():
                update_return = json.loads(api.apply_network(**form.cleaned_data))
                print update_return
                if update_return['result']:
                    network_data = json.loads(api.get_network()).get('data')
                else:
                    action_error[action] = update_return['msg']
            else:
                forms_group[action] = form
        elif action == "AdminPassword":
            form = AdminPassword(request.POST)
            if form.is_valid():
                request.user.set_password(form['password'].data)
                request.user.save()
                return redirect('/logout')
            else:
                forms_group[action] = form
        elif action == "Gateway":
            form = Gateway(request.POST)
            if form.is_valid():
                update_return = json.loads(api.apply_storage_account(**form.cleaned_data))
                print update_return
                if update_return['result']:
                    gateway_data = json.loads(api.get_storage_account()).get('data')
                else:
                    action_error[action] = update_return['msg']
            else:
                forms_group[action] = form
        elif action == "EncryptionKey":
            form = EncryptionKey(request.POST)
            if form.is_valid():
                update_return = json.loads(api.apply_user_enc_key(form['old_key'].data, form['new_key'].data))
                print update_return
                if not update_return['result']:
                    action_error[action] = update_return['msg']
            else:
                forms_group[action] = form

    forms_group['Network'] = forms_group.get('Network', Network(initial=network_data))
    forms_group['Cloud Storage Access'] = forms_group.get('Gateway', Gateway(initial=gateway_data))
    forms_group['Encryption Key'] = forms_group.get('EncryptionKey', EncryptionKey())
    forms_group['Administrator Password'] = forms_group.get('AdminPassword', AdminPassword())

    return render(request, 'dashboard/form_tab.html', {'tab': 'system', 'forms_group': forms_group, 'action_error': action_error})


@login_required
def account(request, action=None):

    forms_group = {}
    gateway_data = json.loads(api.get_storage_account()).get('data')
    action_error = {}
    print gateway_data

    class Gateway(RenderFormMixinClass, forms.Form):
        storage_url = forms.CharField(label='Cloud storage url')
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

    forms_group['Cloud Storage Access'] = forms_group.get('gateway', Gateway(initial=gateway_data))
    forms_group['EncryptionKey'] = forms_group.get('encrypt', EncryptionKey())

    return render(request, 'dashboard/form_tab.html', {'tab': 'account', 'forms_group': forms_group, 'action_error': action_error})


@login_required
def sharefolder(request, action):

    forms_group = {}
    action_error = {}
    smb_data = {"username": json.loads(api.get_smb_user_list()).get('data').get('accounts')}
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

    forms_group['Microsoft Networking Access'] = forms_group.get('smb_setting', SMBSetting(initial=smb_data))

    return render(request, 'dashboard/sharefolder.html', {'tab': 'sharefolder', 'forms_group': forms_group,
                                                          'action_error': action_error, "nfs_data": nfs_data})


@login_required
def sync(request):
    load_data = json.loads(api.get_scheduling_rules())
    data = load_data['data']

    if request.method == "POST":
        query = request.POST
        day = query.get('day')
        bandwidth_option = query.get('bandwidth_option')
        interval_to = query.get('interval_to')
        interval_from = query.get('interval_from')
        bandwidth = query.get('bandwidth')
        no_upload = query.get('no_upload')

        for value in data:
            if value[0] == day:
                now = int(value[0]) - 1
                array = []
                if bandwidth_option == "1":
                    array = [day, 0, 24, -1]
                elif bandwidth_option == "2":
                    if no_upload == "true":
                        array = [day, 0, 24, 0]
                    else:
                        array = [day, 0, 24, bandwidth]
                else:
                    if no_upload == "true":
                        array = [day, interval_from, interval_to, 0]
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

    return render(request, 'dashboard/sync.html', {'tab': 'sync', 'hours': hours, 'weeks_data': weeks_data})


@login_required
def schedule(request):
    load_data = json.loads(api_snapshot.get_snapshot_schedule())
    data = load_data.get('data')
    snapshot_time = data.get('snapshot_time')
    print snapshot_time

    if request.method == "POST":
        query = request.POST
        schedule_radio = query.get('schedule_radio')
        print schedule_radio
        if schedule_radio == "1":
            day = -1
        else:
            day = query.get('select-day')
            day = int(day)
        return_value = json.loads(api_snapshot.set_snapshot_schedule(day))
        return return_value


@login_required
def lifecycle(request):
    load_data = json.loads(api_snapshot.get_snapshot_lifespan())
    data = load_data.get('data')
    default_day = data.get('days_to_live')
    print default_day

    if request.method == "POST":
        query = request.POST
        day = query.get('days')
        day = int(day)
        return_value = json.loads(api_snapshot.set_snapshot_lifespan(day))
        print return_value
        return return_value


@login_required
def syslog(request):
    return_val = json.loads(api.get_gateway_system_log(0, 100, 'gateway'))
    if return_val['result']:
        log_data = return_val['data']

    if request.is_ajax():
        return HttpResponse(json.dumps(log_data['error_log']))

    return render(request, 'dashboard/syslog.html', {'tab': 'syslog',
                                                     'log_data': log_data
                                                     })


@login_required
def snapshot(request, action=None):

    if request.method == "POST":
        if action == "create":
            return_val = json.loads(api_snapshot.take_snapshot())
            if return_val['result']:
                return HttpResponse("Success")
            else:
                return HttpResponse(return_val['msg'], status=500)

        if action == "lifecycle":
            return_val = lifecycle(request)
            if return_val['result']:
                return_hash = get_snapshot_default_value()
                return render(request, 'dashboard/snapshot.html',
                              {'tab': 'lifecycle',
                               #'snapshots': snapshots,
                               'the_day': return_hash.get('the_day'),
                               'default_day': return_hash.get('default_day'),
                               'snapshot_time': return_hash.get('snapshot_time'),
                               'latest_snapshot_time': \
                               return_hash.get('latest_snapshot_time'),
                               }
                              )
                #return HttpResponse("Success: %s" % return_val['msg'])
            else:
                return HttpResponse("Failure: %s" % return_val['msg'], status=500)

        if action == "schedule":
            return_val = schedule(request)
            if return_val['result']:
                return_hash = get_snapshot_default_value()
                return render(request, 'dashboard/snapshot.html',
                              {'tab': 'schedule',
                               #'snapshots': snapshots,
                               'the_day': return_hash.get('the_day'),
                               'default_day': return_hash.get('default_day'),
                               'snapshot_time': return_hash.get('snapshot_time'),
                               'latest_snapshot_time': \
                               return_hash.get('latest_snapshot_time'),
                               })
                #return HttpResponse("Success: %s" % return_val['msg'])
            else:
                return HttpResponse("Failure: %s" % return_val['msg'], status=500)

        if action == "delete":
            snapshot_list = request.POST.getlist("snapshots[]")
            for snap in snapshot_list:
                del_result = json.loads(api_snapshot.delete_snapshot(snap))
                if not del_result['result']:
                    return_val = {'result': False, 'msg': 'An error occurred when deleting %s' % snap}
                    break
            return_val = {'result': True, 'msg': 'All snapshots are deleted.'}

        if action == "export":
            snapshot_list = request.POST.getlist("snapshots[]")
            return_val = json.loads(api_snapshot.expose_snapshot(snapshot_list))

        if return_val['result']:
            return HttpResponse("Success: %s" % return_val['msg'])
        else:
            return HttpResponse("Failure: %s" % return_val['msg'], status=500)

    else:
        return_val = json.loads(api_snapshot.get_snapshot_list())
        snapshots_inprogress = 0
        if return_val['result']:
            snapshots = return_val.get('data').get('snapshots')
            snapshots = sorted(snapshots, key=lambda x: x["start_time"], reverse=True)
        for snapshot in snapshots:
            snapshot['start_time'] = datetime.datetime(*time.localtime(snapshot['start_time'])[0:6])
            snapshot['finish_time'] = datetime.datetime(*time.localtime(snapshot['finish_time'])[0:6]) if snapshot['finish_time'] > 0 else None
            snapshot['total_size'] /= 1000
            if snapshot['name'] == "new_snapshot":
                snapshot['in_progress'] = 1
                snapshots_inprogress += 1
            else:
                snapshot['in_progress'] = 0
            snapshot['path'] = "\\\\" + json.loads(api.get_network())['data']["ip"] + "\\" + snapshot['name'] if snapshot['exposed'] else ""

        if request.is_ajax():
            return render(request, 'dashboard/snapshot_tbody.html', {'tab': 'snapshot', 'snapshots': snapshots})
        else:
            return_hash = get_snapshot_default_value()
            return render(request, 'dashboard/snapshot.html', {'tab': 'snapshot',
                'snapshots': snapshots,
                'snapshots_inprogress': snapshots_inprogress,
                'the_day': return_hash.get('the_day'),
                'default_day': return_hash.get('default_day'),
                'snapshot_time': return_hash.get('snapshot_time'),
                'latest_snapshot_time': return_hash.get('latest_snapshot_time'),
                })


def get_snapshot_default_value():
    return_hash = {'the_day': {}}
    for i in range(0, 24):
        if i < 12:
            val = str(i) + " am"
        else:
            val = str(i - 12) + " pm"

        if val == "0 am":
            val = "12 Midnight"
        elif val == "0 pm":
            val = "12 Noon"

        return_hash['the_day'][i] = val

    load_data = json.loads(api_snapshot.get_snapshot_lifespan())
    data = load_data.get('data')
    return_hash['default_day'] = data.get('days_to_live')

    load_data = json.loads(api_snapshot.get_snapshot_schedule())
    data = load_data.get('data')
    return_hash['snapshot_time'] = data.get('snapshot_time')

    load_data = json.loads(api_snapshot.get_snapshot_last_status())
    return_hash['latest_snapshot_time'] = \
            time.strftime("%a, %d %b %Y %H:%M:%S", \
                    time.gmtime(load_data.get('latest_snapshot_time')))

    return return_hash


@login_required
@require_POST
def http_proxy_switch(request, action=None):
    if request.method == 'POST':
        return_val = json.loads(api_http_proxy.set_http_proxy(action))
        if return_val['result']:
            return HttpResponse(return_val['msg'])
        else:
            return HttpResponse(return_val['msg'], status=500)


@login_required
def system_upgrade(request):
    if request.is_ajax():
        result = api_remote_upgrade.upgrade_gateway()
        return HttpResponse(result)
    else:
        title = 'The system is being upgraded.'
        message = 'Please wait for a while...'
        return render(request, 'dashboard/upgrade.html', {'title': title, 'message': message})


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
def dashboard_update(request):
    data = gateway_status()
    data["version_upgrade"] = json.loads(api_remote_upgrade.get_available_upgrade())["version"]
    return HttpResponse(json.dumps(data))


@login_required
def config(request, action=None):
    if request.method == 'POST':
        if action == 'restore':
            info = api_restore_conf.restore_gateway_configuration()
        elif action == 'save':
            info = api_restore_conf.save_gateway_configuration()
        result = json.loads(info)

        if result['result']:
            info = api_restore_conf.get_configuration_backup_info()
            backup_info = json.loads(info)
            backup_time = datetime.datetime(*time.localtime(int(backup_info['data']['backup_time']))[0:6])
            backup_time_str = datetime.datetime.strftime(backup_time, "%Y-%m-%d %H:%M:%S")
            result['backup_time'] = backup_time_str
        response = json.dumps(result)
        return HttpResponse(response)

    info = api_restore_conf.get_configuration_backup_info()
    result = json.loads(info)
    if result['result']:
        backup_time = datetime.datetime(*time.localtime(int(result['data']['backup_time']))[0:6])
        backup_time_str = datetime.datetime.strftime(backup_time, "%Y-%m-%d %H:%M:%S")
    else:
        backup_time_str = None

    return render(request, 'dashboard/config.html', {'tab': 'config', 'backup_time': backup_time_str})
