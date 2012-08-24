import urllib2
import json
import time


def parse_resp(resp):
    result = {
            'result': False,
            'msg': None,
            'data': None,
    }

    # check HTTP status code
    if resp.getcode() != 200:
        result['msg'] = 'HTTP error: %s' % resp.getcode()
        return result

    # parse to JSON result
    try:
        r = json.load(resp)
    except Exception, e:
        result['msg'] = '%s' % e

    result['result'] = r.get('result', False)
    result['msg'] = r.get('msg')
    result['data'] = r.get('data')
    return result


def urlresult(url, data=None, timeout_sec=None, retry=0, retry_delay_sec=3):
    """
    timeout_sec: time out in sec
    retry: 0 = disable, <0 = always retry, >0 = retry count
    """

    result = {
            'result': False,
            'msg': None,
            'data': None,
    }

    re = 0
    while  True:
        try:
            if re > 0:
                time.sleep(retry_delay_sec)
                print '#%08d retry' % re
            resp = urllib2.urlopen(url, data, timeout = timeout_sec)
            break
        except Exception, e:
            if retry == 0:
                result['msg'] = '%s' % e
                return result
            elif retry < 0: # always retry
                re += 1
            else:
                if re < retry:
                    re += 1
                else:
                    result['msg'] = '%s' % e
                    return result

    return parse_resp(resp)
