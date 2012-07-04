# === demo how to write a wizard form from UI meta
import json
from delta.forms import get_config_form

meta = json.loads(
"""
{
        "cluster_name": {
        "description": "Swift cluster name",
        "type": "text",
        "max": 20,
        "min": 5,
        "required": true,
        "default": "DCloudSwift",
        "order": 0
        },
	
	"portal_domain": {
        "description": "Portal ip or domain name",
        "type": "text",
        "required": true,
        "default": "192.168.11.10",
        "order": 2
        },

        "portal_port": {
        "description": "Portal service port",
        "type": "number",
        "required": true,
        "max": 65535,
        "min": 1,
        "default": 8080,
        "order": 3
        },

        "replica_number": {
        "description": "Nubmber of replica",
        "type": "dropdown",
        "required": true,
        "value": [1, 2, 3, 4, 5],
        "default": 3,
        "order": 4
        }

}
"""
)

MetaForm = get_config_form(meta)
