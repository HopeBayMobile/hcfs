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
        "max": 1048576,
        "min": 1,
        "default": 2048,
        "order": 6
        }

}
"""
)

MetaForm = get_config_form(meta)
