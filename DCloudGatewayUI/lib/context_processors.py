import json

from django.conf import settings


if not getattr(settings, "DEBUG", False):
    from gateway import api
else:
    from gateway.mock import api


def indicator(request):

    indicator_data = json.loads(api.get_gateway_indicators())
    try:
        status_data = json.loads(api.get_gateway_status())
    except:
        status_data = {'data': {"uplink_usage": 100,
                                "downlink_usage": 1000
                               }
                      }
    indicator_data['data'].update(status_data['data'])
    print indicator_data
    return {"result_data": indicator_data['data']}
