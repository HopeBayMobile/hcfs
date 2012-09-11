# === demo how to write a wizard form from UI meta
import json
from delta.forms import get_config_form

meta = json.loads(
"""
{
	"portal_domain": {
        "description": "Portal domain or ip",
        "type": "text",
        "required": true,
        "default": "192.168.11.2",
        "order": 2
        },

        "portal_port": {
        "description": "Portal port",
        "type": "number",
        "required": true,
        "max": 65535,
        "min": 1,
        "default": 8080,
        "order": 3
        },

        "replica_number": {
        "description": "Protection Level",
        "type": "dropdown",
        "required": true,
        "value": [1, 2, 3, 4, 5],
        "order": 4
        },

        "disk_count": {
        "description": "disk count per node",
        "type": "number",
        "required": true,
        "max": 80,
        "min": 1,
        "default": 6,
        "order": 5
        },
       
        "disk_capacity": {
        "description": "capacity per disk (GB)",
        "type": "number",
        "required": true,
        "max": 1000000,
        "min": 1,
        "default": 2000,
        "order": 6
        }

}
"""
)

MetaForm = get_config_form(meta)

from django import forms
from delta.forms import RenderFormMixinClass
from delta.forms import CIDRAddressField
from delta.forms.widgets import *


class ManualForm(RenderFormMixinClass, forms.Form):
    min_ip = forms.IPAddressField(label="min_ip", widget=IPAddressInput)
    max_ip = forms.IPAddressField(label="max_ip", widget=IPAddressInput)
    gateway = forms.IPAddressField(label="gateway", widget=IPAddressInput)
    netmask = forms.IPAddressField(label="netmask", widget=IPAddressInput)


    fieldset = [
                ('Ip range ', [
                                'min_ip',
                                'max_ip',
                                'netmask',
                                'gateway'
                                ]
                )
                ]

    initial_dict = {
        'min_ip': '10.1.4.2',
        'max_ip': '10.1.4.254',

        'netmask': '255.255.255.0',
        'gateway': '192.168.11.254'
    }

