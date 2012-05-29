from gateway import api
import json


def indicator(request):

    indicator_data = json.loads(api.get_gateway_indicators())
    print indicator_data
    return {"result_data": indicator_data['data']}
