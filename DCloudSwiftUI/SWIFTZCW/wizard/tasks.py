from celery.task import task
from delta.wizard.api import DeltaWizardTask
from DCloudSwift.util import util
import DCloudSwift

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


@task(base=DeltaWizardTask)
def do_meta_form(data):
    from time import sleep
    print data["cluster_name"]
    print data["portal_domain"]
    print data["portal_port"]

    hosts = do_meta_form.get_zone_hosts()

    hosts = assign_swift_zid(hosts=hosts, replica_number=int(data["replica_number"]))
    if hosts is None:
        raise Exception("Replica number > number of hosts!!")
    do_meta_form.report_progress(10, True, 'Calculate swift zone id for each host', None)
    
    hosts = dns_lookup(hosts=hosts)
    for host in hosts:
        if host["ip"] is None:
            raise Exception("Failed to lookup the ip of %s" % host["hostname"])
    do_meta_form.report_progress(20, True, 'Lookup IPs of hosts', None)

    #raise Exception('test task fail')


