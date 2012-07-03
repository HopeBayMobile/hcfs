# === demo how to write a wizard form from UI meta
import json
from delta.forms import get_config_form

meta = json.loads(
"""
{
	"admin_name": {
	"description": "Input field for single line text",
	"type": "text",
	"max": 10,
	"min": 3,
	"required": true,
	"default": "openstack",
	"order": 0
	},

	"admin_pass": {
	"description": "Input field for password",
	"type": "pass",
	"max": 10,
	"min": 3,
	"required": true,
	"default": "admin",
	"order": 1
	},

	"memo": {
	"description": "Input field for multiline text",
	"type": "textarea",
	"max": 300,
	"min": 0,
	"required": true,
	"default": "memo",
	"order": 3
	},

	"memo2": {
	"description": "Input field for multiline text (memo2)",
	"type": "textarea",
	"max": 300,
	"min": 0,
	"required": false,
	"default": "memo",
	"order": 0
	},

	"bool": {
	"description": "Input field for boolean value",
	"type": "bool",
	"required": true,
	"default": true,
	"order": 2
	},

	"radio": {
	"description": "Input field for radio option",
	"type": "radio",
	"required": true,
	"value": ["value1", "value2", "value3"],
	"default": "value3",
	"order": 3
	},

	"checkbox": {
	"description": "Input field for checkbox option",
	"type": "checkbox",
	"required": true,
	"value": ["value1", "value2", "value3"],
	"default": ["value3", "value2"],
	"min": 1,
	"max": 2,
	"order": 4
	},

	"dropdown": {
	"description": "Input field for dropdown option",
	"type": "dropdown",
	"required": true,
	"value": ["value1", "value2", "value3"],
	"default": "value3",
	"order": 5
	},

	"number": {
	"description": "Input field for number",
	"type": "number",
	"required": true,
	"max": 50,
	"min": 0,
	"step": 1,
	"default": 2,
	"order": 6
	},

	"slider": {
	"description": "Input field for range slider",
	"type": "slider",
	"required": true,
	"max": 50,
	"min": 1,
	"step": 1,
	"default": 1,
	"order": 7
	}
}
"""
)

MetaForm = get_config_form(meta)

# ----------------------------------------------------------------------------
# === demo how to write a UI form manually

from django import forms
from delta.forms import RenderFormMixinClass
from delta.forms import CIDRAddressField
from delta.forms.widgets import *

class ManualForm(RenderFormMixinClass, forms.Form):
    password = forms.CharField(widget=forms.PasswordInput(), min_length=4)
    retype_password = forms.CharField(widget=forms.PasswordInput(), min_length=4)

    alloc_subnet = CIDRAddressField(label="Allocated Subnet", widget=CIDRAddressInput)
    tmp_subnet = CIDRAddressField(label="Temp Subnet", widget=CIDRAddressInput)
    gateway = forms.IPAddressField(widget=IPAddressInput)
    dns_server = forms.IPAddressField(label="DNS Server", widget=IPAddressInput)

    storage_ip = forms.IPAddressField(widget=IPAddressInput)
    storage_port = forms.CharField()
    storage_iscsi_target = forms.CharField()

    fieldset = [('1 Set admin password', [
                                'password',
                                'retype_password',
                                ]
                ),
                ('2 Network Configuration', [
                                'alloc_subnet',
                                'tmp_subnet',
                                'gateway',
                                'dns_server'
                                ]
                ),
                ('3 Storage Configuration', [
                                'storage_ip',
                                'storage_port',
                                'storage_iscsi_target'
                                ]
                )]

    initial_dict = {
        'password': '1234',
        'retype_password': '1234',

        'alloc_subnet': '192.168.11.0/24',
        'tmp_subnet': '192.168.12.0/24',
        'gateway': '192.168.11.254',
        'dns_server': '192.168.11.1',

        'storage_ip': '192.168.11.1',
        'storage_port': 3133,
        'storage_iscsi_target': 'IQN'
    }

    def clean(self):
        cleaned_data = super(ManualForm, self).clean()
        password = cleaned_data.get('password')
        retype_password = cleaned_data.get('retype_password')
        if password and retype_password:
            if password != retype_password:
                self._errors['retype_password'] = self.error_class(["Passwords don't match."])
        return cleaned_data
