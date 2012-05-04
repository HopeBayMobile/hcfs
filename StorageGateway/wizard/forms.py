from django import forms
from lib.forms import RenderFormMixinClass
from lib.forms.widgets import *

class Form_1(RenderFormMixinClass, forms.Form):
    username = forms.CharField()
    password = forms.CharField(widget=forms.PasswordInput())
    new_password = forms.CharField(widget=forms.PasswordInput())
    retype_new_password = forms.CharField(widget=forms.PasswordInput())
    fieldset = [('1.Setup your new password.', ['username',
                                                'password',
                                                'new_password',
                                                'retype_new_password',
                                                ]
                 )]
    def clean(self):
        cleaned_data = self.cleaned_data
        if cleaned_data['new_password'] != cleaned_data['retype_new_password']:
            self._errors['retype_new_password']=self.error_class(["New passwords don't match."])
            #raise forms.ValidationError("New passwords don't match.")
        return cleaned_data

class Form_2(RenderFormMixinClass, forms.Form):
    ip_address = forms.IPAddressField(label='IP address')
    subnet_mask = forms.IPAddressField()
    default_gateway = forms.IPAddressField()
    preferred_dns = forms.IPAddressField(label='Preferred DNS server')
    alternate_dns = forms.IPAddressField(label='Alternate DNS server')
    fieldset = [('2.Setup Network.', ['ip_address',
                                     'subnet_mask',
                                     'default_gateway',
                                     'preferred_dns',
                                     'alternate_dns',
                                     ]
                 )]
    
class Form_3(RenderFormMixinClass, forms.Form):
    cloud_storage_url = forms.CharField(label='Cloud Storage URL')
    account = forms.CharField()
    password = forms.CharField(widget=forms.PasswordInput())
    fieldset = [('3.Setup Cloud Storage', ['cloud_storage_url',
                                           'account',
                                           'password',
                                           ]
                 )]
    
class Form_4(RenderFormMixinClass, forms.Form):
    encryption_key = forms.CharField(help_text='Input 6-20 alphanumeric characters',
                                     min_length=6,
                                     max_length=20,
                                     widget=forms.PasswordInput())
    confirm_encryption_key = forms.CharField(min_length=6,
                                             max_length=20,
                                             widget=forms.PasswordInput())
    fieldset = [('4.Setup Encryption', ['encryption_key',
                                        'confirm_encryption_key',
                                        ]
                 )]
