# === demo how to write a wizard form from UI meta
import json
from delta.forms import get_config_form

meta = json.loads(
"""
{
	"portal_domain": {
        "description": "Portal domain or ip (public ip)",
        "type": "text",
        "required": true,
        "default": "10.2.4.4",
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
        },

        "min_ip": {
        "description": "min available ip",
        "type": "text",
        "required": true,
        "default": "10.2.4.2",
        "order": 7
        },

        "max_ip": {
        "description": "max available ip",
        "type": "text",
        "required": true,
        "default": "10.2.4.254",
        "order": 8
        },

        "netmask": {
        "description": "netmask",
        "type": "text",
        "required": true,
        "default": "255.255.255.0",
        "order": 9
        },

        "gateway": {
        "description": "gateway",
        "type": "text",
        "required": true,
        "default": "10.2.4.1",
        "order": 10
        },

        "zcw_ip": {
        "description": "zcw ip",
        "type": "text",
        "required": true,
        "default": "10.2.4.3",
        "order": 11
        },

        "zcw_netmask": {
        "description": "zcw_netmask",
        "type": "text",
        "required": true,
        "default": "255.255.255.0",
        "order": 12
        },

        "zcw_gateway": {
        "description": "zcw_gateway",
        "type": "text",
        "required": true,
        "default": "10.2.4.254",
        "order": 13
        }
}
"""
)

MetaForm = get_config_form(meta)
