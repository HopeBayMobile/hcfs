import os
import sys
import socket
import time
import json
import subprocess
import threading
import datetime
import logging
import pickle
import collections
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser
from threading import Thread

#Self defined packages
WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
sys.path.append("%s/DCloudSwift/" % BASEDIR)

from util import util
from util import threadpool
from util.SwiftCfg import SwiftCfg
from util.util import GlobalVar
from util.SwiftCfg import SwiftMasterCfg

DEFAULT_DNS_CONF_CONTENTS = '''zone "dcloudswift" {
type master;
file "%s";
};
''' % GlobalVar.DNS_DB

DEFAULT_DNS_DB_CONTENTS = '''$TTL 60
@ IN SOA localhost. root.localhost. (
                     1          ; Serial
                 86400          ; Refresh
                  900           ; Retry
                2419200         ; Expire
                 604800 )       ; Negative Cache TTL
;
@ IN NS localhost.
'''

class SwiftLoadBalancer:

    def __init__(self, conf=GlobalVar.MASTERCONF):
        self.ip = util.getIpAddress()

        if os.path.isfile(conf):
            self.masterCfg = SwiftMasterCfg(GlobalVar.MASTERCONF)
        else:
            msg = "Confing %s does not exist" % conf
            print >> sys.stderr, msg
            logger.warn(msg)

        if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
            os.system("echo \"StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

    def setupDnsRoundRobin(self, proxyList, domainName=None):
        '''
            setup dns round robin for sepcified proxy nodes
            @type  proxyList: list
            @param proxyList: a list of proxy nodes
            @type  domainName: string
            @param domainName: domainName used for proxy nodes
            @return: {"code": <0:ok>|<1:failed>, "message": string}
        '''
        logger = util.getLogger(name="SwiftLoadBalancer.setupDnsRoundRobin")

        try:
            dns_conf_contents = DEFAULT_DNS_CONF_CONTENTS
            dns_db_contents = DEFAULT_DNS_DB_CONTENTS
            ip_list = ["\n@  IN A "+node['ip'] for node in proxyList]
            
            dns_db_contents = dns_db_contents + "".join(ip_list)
            if domainName:
                dns_conf_contents = dns_conf_contents.replace("dcloudswift", domainName, 1)

            with open(GlobalVar.DNS_DB, "w") as fh:
                fh.write(dns_db_contents)

            with open("/etc/bind/named.conf.local", "w") as fh:
                fh.write(dns_conf_contents)

            return {"code": 0, "message": ""}

        except Exception as e:
            logger.error(str(e))
            return {"code":1, "message": str(e)}


def getSection(inputFile, section):
    ret = []
    with open(inputFile) as fh:
        lines = fh.readlines()
        start = 0
        for i in range(len(lines)):
            line = lines[i].strip()
            if line.startswith('[') and section in line:
                start = i + 1
                break
        end = len(lines)
        for i in range(start, len(lines)):
            line = lines[i].strip()
            if line.startswith('['):
                end = i
                break

        for line in lines[start:end]:
            line = line.strip()
            if len(line) > 0:
                ret.append(line)

        return ret


def parseBalanceSection(inputFile):
    lines = getSection(inputFile, "balance")
    try:
        proxyList = []
        ipSet = set()
        for line in lines:
            line = line.strip()
            if len(line) > 0:
                tokens = line.split()
                try:
                    ip = tokens[0]
                    socket.inet_aton(ip)

                    if ip in ipSet:
                        raise Exception("[balanceNodes] contains duplicate ip")

                    proxyList.append({"ip": ip})
                    ipSet.add(ip)
                except socket.error:
                    raise Exception("[balanceNodes] contains an invalid ip %s" % ip)

        return proxyList
    except IOError as e:
        msg = "Failed to access input files for %s" % str(e)
        raise Exception(msg)


def setupDnsRoundRobin():
    '''
    Command line implementation of dcloud_loadbalancer_setup
    '''

    ret = 1

    Usage = '''
    Usage:
        dcloud_swift_dns_setup swift_proxy_domain_name
    arguments:
        None
    Examples:
        dcloud_swift_dns_setup proxy.dcloudswift
    '''

    if (len(sys.argv) != 2):
        print >> sys.stderr, Usage
        sys.exit(1)

    inputFile = "/etc/delta/inputFile"
    domainName = sys.argv[1]
    try:
        proxyList = parseBalanceSection(inputFile)
        slb = SwiftLoadBalancer()
        result = slb.setupDnsRoundRobin(proxyList, domainName=domainName)
        ret = result["code"]
        if ret !=0 :
            print >>sys.stderr, result["message"]
        else:
            os.system("/etc/init.d/bind9 stop")
            os.system("/etc/init.d/bind9 start")
    except Exception as e:
        print >> sys.stderr, str(e)
    finally:
        return ret

if __name__ == '__main__':
    setupDnsRoundRobin()

