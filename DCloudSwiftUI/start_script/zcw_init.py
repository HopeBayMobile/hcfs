import os
import urllib
import json
from datetime import datetime
from urlutils import urlresult
from pprint import pprint

DEBUG_HOSTNAME = None

PDCM_BASE_URL = 'http://192.168.11.1'
ZCW_BASE_URL = 'http://localhost:8765'
ZCW_SERVICE_SCRIPT = '/usr/local/bin/zcw'


def gethostname():
    import socket
    hostname = socket.gethostname()
    if DEBUG_HOSTNAME:
        return DEBUG_HOSTNAME
    else:
        return hostname


def log(msg):
    print '[%s] %s' % (datetime.now(), msg)


def err_exit(msg):
    import sys
    sys.exit('[%s] %s\nexits with error.' % (datetime.now(), msg))


def get_info_url():
    return '%s/zcw/node/%s/info' % (PDCM_BASE_URL, gethostname())


def get_ready_url():
    return '%s/zcw/node/%s/ready' % (PDCM_BASE_URL, gethostname())


def get_zcw_status_url():
    return '%s/wizard/status' % ZCW_BASE_URL


def send_ready(result, msg=None, zcw=None):
    result = {
            'result': result,
            'msg': msg,
            'data': None,
    }

    if zcw:
        result['data'] = zcw

    try:
        json_str = json.dumps(result)
    except Exception, e:
        return {
                'result': False,
                'msg': '%s' % e,
                'data': None
        }

    log('sending ready signal to PDCM via %s' % get_ready_url())
    resp_result = urlresult(get_ready_url(), json_str, retry=-1)

    if not resp_result.get('result'):
        err_exit('failed. reason: %s' % resp_result.get('msg'))
    else:
        log('success sending ready signal')


def save_zcw_hosts(hosts):
    ZCW_DIR = '/var/zcw'
    ZCW_HOSTS = '/var/zcw/hosts'

    try:
        import os
        os.makedirs(ZCW_DIR)
    except Exception, e:
        if e.errno != 17:
            err_exit('can not create /var/zcw folder for storing hosts info. reason: %s' % e)
    try:
        f = open(ZCW_HOSTS, 'w+')
    except Exception, e:
        err_exit('can not open /var/zcw/hosts storing hosts info. reason: %s' % e)

    f.write(json.dumps(hosts))
    f.close()
    log('save hosts info into /var/zcw/hosts')

def save_zcw_master(master):
    ZCW_DIR = '/var/zcw'
    ZCW_IP = '/var/zcw/master'

    try:
        import os
        os.makedirs(ZCW_DIR)
    except Exception, e:
        if e.errno != 17:
            err_exit('can not create /var/zcw folder for storing ip of zcw. reason: %s' % e)
    try:
        f = open(ZCW_IP, 'w+')
    except Exception, e:
        err_exit('can not open /var/zcw/ip storing ip of zcw. reason: %s' % e)

    f.write(json.dumps(master))
    f.close()
    log('save zcw master into /var/zcw/master')

def start_zcw_web_service():
    from os import system
    r = system('%s start' % ZCW_SERVICE_SCRIPT)
    return r == 0
    

def pull_zcw_status():
    retry = 0
    while True:
        result = urlresult(get_zcw_status_url(), timeout_sec = 30, retry=-1)
        if result['result']:
            return result
        else:
            retry += 1
            if retry == 3:
                return result


def main():
    # get zcw info via url
    log('fetching node zcw information from %s ...' % get_info_url())
    info = urlresult(get_info_url(), retry=-1)

    if not info.get('result'):
        err_exit('failed, reason: %s' % info.get('msg', 'unknown error'))
        
    data = info.get('data')
    if not data:
        err_exit('field "data" is not found in node info.')
    zoneid = data.get('zone', 'unknown zone id')
    log('current node belongs to zone "%s"' % zoneid)

    is_zcw = data.get('is_zcw')
    if is_zcw:
        log('current node is zcw node.')
        hosts = data.get('hosts', [])

        if len(hosts) == 0:
            err_exit('no host found in zone "%s"', zoneid)
        else:
            log('listing zone hosts ...')
            for h in hosts:
                log('%s - Island-%s, Rack-%s, Position-%s' % (h.get('hostname'), h.get('island', 'unknown'), h.get('rack', 'unknown'), h.get('position', 'unknown')))

            # save zcw hosts file
            save_zcw_hosts(hosts)

            # start zcw web service
            start_zcw_web_service()

            # check if service really start
            zcw_status_result = pull_zcw_status()

            if zcw_status_result['result']:
                send_ready(True, zcw=zcw_status_result.get('data'))
            else:
                send_ready(False, 'can not start zcw web service in zone "%s". resason: %s' % (zoneid, zcw_status_result.get('msg', 'unknown reason')))

            # added by Ken
            cmd = "mkdir /tmp/i_am_zcw"
            if not os.path.exists("/tmp/i_am_zcw"):
                os.system(cmd)
    else:
        log('current node is not zcw node.')
        zcw_hostname = data.get('zcw_hostname', '')

        if not zcw_hostname:
            hosts = data.get('hosts', [])
            if len(hosts) > 0:
                zcw_hostname = hosts[0].get('hostname', '')

        log('zcw hostname is %s' % zcw_hostname)
        master = {"hostname": zcw_hostname}
        # save zcw master
        save_zcw_master(master)

        send_ready(True)

    log('zcw starter script end normally')

if __name__ == '__main__':
    main()
