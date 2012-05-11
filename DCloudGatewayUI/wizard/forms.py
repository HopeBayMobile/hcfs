from django import forms
from django.forms.widgets import Input
from django.forms.util import flatatt
from django.utils.safestring import mark_safe
from django.utils.encoding import StrAndUnicode, force_unicode
from lib.forms import RenderFormMixinClass
from lib.forms.widgets import *

class Form_1(RenderFormMixinClass, forms.Form):
    username = forms.CharField()
    password = forms.CharField(widget=forms.PasswordInput())
    new_password = forms.CharField(widget=forms.PasswordInput())
    retype_new_password = forms.CharField(widget=forms.PasswordInput())
    fieldset = [('1. Setup your new password', ['username',
                                                'password',
                                                'new_password',
                                                'retype_new_password',
                                                ]
                 )]
    def clean(self):
        cleaned_data = super(Form_All, self).clean()
        new_password = cleaned_data.get('new_password')
        retype_new_password = cleaned_data.get('retype_new_password')
        if new_password and retype_new_password:
            if new_password != retype_new_password:
                self._errors['retype_new_password'] = self.error_class(["New passwords don't match."])
        return cleaned_data

class Form_2(RenderFormMixinClass, forms.Form):
    ip_address = forms.IPAddressField(label='IP address')
    subnet_mask = forms.IPAddressField()
    default_gateway = forms.IPAddressField()
    preferred_dns = forms.IPAddressField(label='Preferred DNS server')
    alternate_dns = forms.IPAddressField(label='Alternate DNS server')
    fieldset = [('2. Setup Network', ['ip_address',
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
    fieldset = [('3. Setup Cloud Storage', ['cloud_storage_url',
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
    fieldset = [('4. Setup Encryption', ['encryption_key',
                                        'confirm_encryption_key',
                                        ]
                 )]
    
class IPAddressInput(Input):
    input_type = 'text'
    separator = u'.'
    input_number = 4

    def value_from_datadict(self, data, files, name):
        """
        Given a dictionary of data and this widget's name, returns the value
        of this widget. Returns None if it's not provided.
        """
        input_list = data.getlist(name)
        data = self.separator.join(input_list)
        if any(input_list):
            return data
        else:
            return None
    
    def render(self, name, value, attrs=None):
        if value is None:
            value = ''
        final_attrs = self.build_attrs(attrs, type=self.input_type, name=name)
        final_attrs['class'] = 'span05'
        
        value_list = [''] * self.input_number
        if value != '':
            value_list = value.split(self.separator)

        #create 4 input for IP address
        input_list = []
        for i in range(self.input_number):
            final_attrs['value'] = force_unicode(self._format_value(value_list[i]))
            if not value_list[i]:
                del final_attrs['value']
            input_list.append(u'<input%s />' % flatatt(final_attrs))
        render_mark = self.separator.join(input_list)
        
        return mark_safe(render_mark)
 
class Form_All(RenderFormMixinClass, forms.Form):
#    username = forms.CharField()
#    password = forms.CharField(widget=forms.PasswordInput())
    new_password = forms.CharField(widget=forms.PasswordInput())
    retype_new_password = forms.CharField(widget=forms.PasswordInput())
    
    ip_address = forms.IPAddressField(label='IP address', widget=IPAddressInput)
    subnet_mask = forms.IPAddressField(widget=IPAddressInput)
    default_gateway = forms.IPAddressField(widget=IPAddressInput)
    preferred_dns = forms.IPAddressField(label='Preferred DNS server', widget=IPAddressInput)
    alternate_dns = forms.IPAddressField(label='Alternate DNS server', widget=IPAddressInput)
    
    cloud_storage_url = forms.CharField(label='Cloud Storage IP')
    cloud_storage_account = forms.CharField()
    cloud_storage_password = forms.CharField(widget=forms.PasswordInput())
    
    encryption_key = forms.CharField(help_text='Input 6-20 alphanumeric characters',
                                     min_length=6,
                                     max_length=20,
                                     widget=forms.PasswordInput())
    confirm_encryption_key = forms.CharField(min_length=6,
                                             max_length=20,
                                             widget=forms.PasswordInput())
    
    fieldset = [('1. Password', [#'username',
                                #'password',
                                'new_password',
                                'retype_new_password',
                                ]
                 ),
                ('2. Network', ['ip_address',
                                'subnet_mask',
                                'default_gateway',
                                'preferred_dns',
                                'alternate_dns',
                                ]
                 ),
                ('3. Cloud Storage', ['cloud_storage_url',
                                     'cloud_storage_account',
                                     'cloud_storage_password',
                                     ]
                 ),
                ('4. Encryption', ['encryption_key',
                                  'confirm_encryption_key',
                                  ]
                 )]

    def clean(self):
        cleaned_data = super(Form_All, self).clean()
        new_password = cleaned_data.get('new_password')
        retype_new_password = cleaned_data.get('retype_new_password')
        if new_password and retype_new_password:
            if new_password != retype_new_password:
                self._errors['retype_new_password'] = self.error_class(["New passwords don't match."])
        return cleaned_data
