import os
import urllib
import json
import subprocess
from datetime import datetime
from urlutils import urlresult
from pprint import pprint

DEBUG_HOSTNAME = None

PDCM_BASE_URL = 'http://192.168.11.1'
ZCW_BASE_URL = 'http://localhost:8765'
ZCW_SERVICE_SCRIPT = '/usr/local/bin/zcw'

def lookup_ip(hostname, nameserver="192.168.11.1"):
    '''
    lookup ip of hostname by asking nameserver.

    @type  hostname: string
    @param hostname: the hostname to be translated
    @type  nameserver: string
    @param nameserver: ip address of the nameserver, and the default
        value is 192.168.11.1
    @rtype: string
    @return: If the translation is successfully done, then the ip
        address will be returned. Otherwise, the returned value will
        be an empty string.
    '''

    ip = ""
    cmd = "host %s %s" % (hostname, nameserver)
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    (stdoutData, stderrData) = po.communicate()

    if po.returncode == 0:
        lines = stdoutData.split("\n")

        for line in lines:
            if hostname in line:
                ip = line.split()[3]
                if ip.find("255.255.255.255") != -1:
                    ip = ""

    return ip

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

def save_zcw_contact_info(contact_info):
    ZCW_DIR = '/var/zcw'
    CONTACT_INFO_PATH = '/var/zcw/contact_info'

    try:
        import os
        os.makedirs(ZCW_DIR)
    except Exception, e:
        if e.errno != 17:
            err_exit('can not create folder for storing contact info of zcw. reason: %s' % e)
    try:
        f = open(CONTACT_INFO_PATH, 'w+')
    except Exception, e:
        err_exit('can not open %s storing info of zcw. reason: %s' % (CONTACT_INFO_PATH, e))

    f.write(json.dumps(contact_info))
    f.close()
    log('save zcw contact into %s' % CONTACT_INFO_PATH)

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
        zcw_ip = ""

        # take the fisrt host as zcw
        if not zcw_hostname:
            hosts = data.get('hosts', [])
            if len(hosts) > 0:
                zcw_hostname = hosts[0].get('hostname', '')

        if zcw_hostname:
            zcw_ip = lookup_ip(zcw_hostname)
            if zcw_ip:
                cmd = "echo %s zcw >> /etc/hosts" % zcw_ip
                os.system(cmd)

        log('zcw hostname is %s' % zcw_hostname)
        log('zcw ip is %s' % zcw_ip)
        contact_info = {"hostname": zcw_hostname, 'ip': zcw_ip}
        # save zcw master
        save_zcw_contact_info(contact_info)

        send_ready(True)

    log('zcw starter script end normally')

if __name__ == '__main__':
    main()
