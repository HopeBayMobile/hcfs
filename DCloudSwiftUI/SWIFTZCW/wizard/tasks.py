import ConfigParser

from celery.task import task
from delta.wizard.api import DeltaWizardTask
from DCloudSwift.util import util
import DCloudSwift


SWIFTCONF = util.GlobalVar.ORI_SWIFTCONF

def dns_lookup(hosts, nameserver="192.168.11.1"):
    '''
    lookup ip addresses of hosts.

    @type  hosts: list of hosts, each host is dict containing key "hostname"
    @param hosts: the hosts to lookup ips
    @type  nameserver: string
    @param nameserver: ip address of the nameserver, and the default
        value is 192.168.11.1
    @rtype: a list of hosts and each host is a dictinary containing an additional key "ip"
    @return: The input list hosts is shallow copied and a new key "ip" is added to each host.
        If the ip lookup of a host is successfully done, then the founed ip is set 
        to the value of key "ip" for that host. Otherwise None is used.
    '''

    ret = []
    for host in hosts:
        mutable_host = host.copy()
        hostname = mutable_host["hostname"]
        ip = util.hostname2Ip(hostname)  # This function returns None if lookup failed
        mutable_host[u'ip'] = ip
        ret.append(mutable_host)

    return ret


def assign_swift_zid(hosts, replica_number):
    '''
    assign a swift zone id to each host in hosts.

    @type  hosts: list of hosts, each host is dict containing keys "hostname", "island" and "rack"
    @param hosts: the hosts to assign zids
    @type  replica_number: integer
    @param replica_number: number of replica
    @rtype: a list of hosts and each host is a dictinary containing an additional key "zid"
    @return: The input list hosts is shallow copied and a new key "zid" is added to each host.
        If replica_number < number of hosts, then None is returned.
    '''

    if len(hosts) < replica_number:
        return None

    zone_number = min(len(hosts), replica_number+2)
    group_size = len(hosts)/zone_number
    ret = []

    for i in range(len(hosts)):
        zid = (i/group_size) + 1 if i < group_size * zone_number else (i % zone_number) + 1
        host = hosts[i].copy()
        host[u'zid'] = zid
        ret.append(host)

    return ret


def set_portal_url(portal_url):
    '''
    @type  portal_url: string"
    @param portal_url: url of portal
    @rtype: None
    @return: no return
    '''
    config = ConfigParser.ConfigParser()
    section = "portal"

    try:
        with open(SWIFTCONF) as fh:
            config.readfp(fh)
    except IOError:
        raise Exception("Failed to access %s" % SWIFTCONF)

    if not config.has_section(section):
        raise Exception("There is no [portal] section in the swift config")

    config.set(section, 'url', portal_url)

    try:
        with open(SWIFTCONF, 'wb') as fh:
            config.write(fh)
    except IOError:
        raise Exception("Failed to access %s" % SWIFTCONF)


@task(base=DeltaWizardTask)
def do_meta_form(data):
    from time import sleep
    print data["cluster_name"]

    #  Contruct URL of portal and write it to SWIFTCONF
    if data["portal_port"] < 1 or data["portal_port"] > 65536:
        raise Exception("Portal port has to be an ingeger within the range [1, 65536]")
    portal_url = "https://"+data["portal_domain"]+":"+str(data["portal_port"])
    set_portal_url(portal_url=portal_url)

    # Get list of hosts
    hosts = do_meta_form.get_zone_hosts()

    # Assign swift zone id for each host
    hosts = assign_swift_zid(hosts=hosts, replica_number=int(data["replica_number"]))
    if hosts is None:
        raise Exception("Replica number > number of hosts!!")
    do_meta_form.report_progress(10, True, 'Calculate swift zone id for each host', None)
    
    # Lookup ip of each host
    hosts = dns_lookup(hosts=hosts)
    for host in hosts:
        if host["ip"] is None:
            raise Exception("Failed to lookup the ip of %s" % host["hostname"])
    do_meta_form.report_progress(20, True, 'Lookup IPs of hosts', None)

    #raise Exception('test task fail')


