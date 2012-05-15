from django.shortcuts import render
from django.contrib.auth.decorators import login_required
from django import forms
from lib.forms import RenderFormMixinClass
from lib.forms import IPAddressInput

@login_required
def index(request):
    return render(request, 'dashboard/content.html')


def system(request):

    class Network(RenderFormMixinClass, forms.Form):
        ip_address = forms.IPAddressField(label='IP Address', widget=IPAddressInput)
        gateway = forms.IPAddressField(label='Gateway Address', widget=IPAddressInput)
        subnet_mask = forms.IPAddressField(label='Submask', widget=IPAddressInput)
        primary_dns = forms.IPAddressField(label='Primary DNS server', widget=IPAddressInput)
        secondary_dns = forms.IPAddressField(label='Secondary DNS server', widget=IPAddressInput)

    class AdminPassword(RenderFormMixinClass, forms.Form):
        password = forms.CharField(widget=forms.PasswordInput)
        retry_password = forms.CharField(widget=forms.PasswordInput)

    forms_group = [Network(), AdminPassword()]

    # if request.method == "POST":
    #     form = Network(request.POST)
    # else:
    #     pass
    return render(request, 'dashboard/content.html', {'tab': 'system', 'forms_group': forms_group})


@login_required
def account_configuration(request):
    return render(request, 'account_configuration.html')


@login_required
def sync_management(request):
    return render(request, 'sync_management.html')


@login_required
def system_log(request):
    return render(request, 'system_log.html')
