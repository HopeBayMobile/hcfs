#!/usr/bin/env python
'''
Created on Aug 1, 2011

@author: juitsung
'''
import sys
try:
    import pexpect
except:
    print "No package. (pexpect) apt-get install python-pexpect."
    sys.exit()
import re

__VERSION__ = "version: 0.1"
__DEBUG__ = False
__TIMEOUT__ = 60
'''
function: clearHostsHD
parms: addr as list
return: none 
'''
def clearHostsHD(hosts, action = 'reboot'):
    for host in hosts:
        if host.has_key('addr') == False:
            continue
        addr = host['addr']
        user = host['user']
        password = host['password']
        print "*** host processing: %s" % (addr)
        if(__DEBUG__ == False):
            print ('** after clear, action: %s', action)
            cmd = 'ssh %s@%s "dd if=/dev/zero of=/dev/sda bs=512 count=1;sleep 1;%s"' % (user, addr, action)
            ssh = pexpect.spawn(cmd)
            try:
                i = ssh.expect(['password', 'continue connecting(yes/no)?'], __TIMEOUT__)
                if i == 0:
                    ssh.sendline(password)
                elif i == 1:
                    ssh.sendline('yes')
                ssh.sendline("\n")
                print "*** host finished: %s" % addr
            except pexpect.EOF:
                print "*** host fail: %s" % addr
            except pexpect.TIMEOUT:
                print "*** host timeout: %s" % addr
            ssh.close()

def host_exists(hosts, host):
    result = False
    if len(hosts) > 1 and host.has_key('addr') == False:
        return result
    for h in hosts:
        if h['addr'] == host['addr']:
            result = True
            break
    return result

def create_host_table(first_addr, final_addr, count = 0):
    hosts = []
    ipv4_pattern = r"\b(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\b"
    addr1 = re.match(ipv4_pattern, first_addr)
    ip1 = int(addr1.group(4))
    if(final_addr != ''):
        addr2 = re.match(ipv4_pattern, final_addr)
        ip2 = int(addr2.group(4))
    elif count > 1:
        ip2 = ip1 + count - 1
    network = ("%s.%s.%s") % (addr1.group(1),addr1.group(2),addr1.group(3))
    ip = ip1
    while ip <= ip2:
        host = {}
        host['addr'] = ("%s.%d") % (network, ip)
        ip = ip + 1
        hosts.append(host)
    return hosts

def readFile(filename):
    file = open(filename)
    lines = file.read()
    hosts = []
    username = ''
    password = ''
    action = ''
    first_addr = ''
    final_addr = ''
    count = 0
    for line in lines.split("\n"):
        match1 = re.match('(.*)[\s\t]=[\s\t](.*)$', line)
        if line.startswith('#'):
            continue
        elif line == '':
            continue
        elif match1:
            field = match1.group(1)
            value = match1.group(2)
            if field == 'timeout':
                __TIMEOUT__ = int(value)
            elif field == 'username':
                username = value
            elif field == 'password':
                password = value
            elif field == 'action':
                action = value
            elif field == 'first_addr':
                first_addr = value
            elif field == 'final_addr':
                final_addr = value
            elif field == 'count':
                count = int(value)
            if first_addr != '' and (final_addr != '' or count > 0):
                hosts = create_host_table(first_addr, final_addr, count)
        else:
            host = {}
            row = re.match('(.*)[\s\t](.*)[\s\t](.*)', line)
            if row:
                host['addr'] = row.group(1).strip()
                host['user'] = row.group(2).strip()
                host['password'] = row.group(3).strip()
            else:
                row = re.match('(.*)[\s\t](.*)', line)
                if row:
                    host['addr'] = row.group(1)
                    host['user'] = row.group(2)
                else:
                    host['addr'] = line.strip()
            if host_exists(hosts, host) != True:
                hosts.append(host)
    # end of for line in lines.split("\n"):
    idx = 0
    if username != '' and password != '':
        while idx < len(hosts):
            if hosts[idx].has_key('user') == False: 
                hosts[idx]['user'] = username
            if hosts[idx].has_key('password') == False:
                hosts[idx]['password'] = password
            idx = idx + 1
    hosts.sort(reverse=False)
    
    if(__DEBUG__):
        print '*** DEBUG: hosts=%s' % hosts
    return hosts, action

def print_help():
    usage = '''\
Command line:
    clearRemoteHD.py [-f <file>] [-ip <ipv4addr>] [-u <user>] [-p <password>] [--version] [--help] [-r|-s] [-t]
Options include:
    --version: Print the version.
    --help: Display help.
    -f <filename>: hosts' address list file, example: host.example.
    -ip <ipv4addr>: host's address.
    -u <username>: super username.
    -p <password>: password.
    -r : host reboot.
    -s : host shutdown.
    -t : timeout
'''
    print usage
    sys.exit()
    
def display_version():
    print __VERSION__
    sys.exit()

def argument_parser():
    hosts = []
    action = 'reboot'
    username = ''
    password = ''
    idx = 1
    
    if sys.argv[idx].startswith('--'):
        option = sys.argv[idx][2:]
        if option == "version":
            display_version()
        elif option == "help":
            print_help()
    
    while idx < len (sys.argv):
        option = sys.argv[idx]
        if option == "-f":
            idx = idx + 1
            filename = sys.argv[idx]
            hosts, action = readFile(filename)
            if action == '':
                action = 'reboot'
            elif action == 'shutdown' or action == 'poweroff':
                action = 'halt'
        elif option == "-ip":
            host = {}
            idx = idx + 1
            addr = sys.argv[idx]
            host['addr'] = addr
            hosts.append(host)
        elif option == "-r":
            action = 'reboot'
        elif option == "-s":
            action = 'halt'
        elif option == "--debug":
            __DEBUG__ = True
        elif option == "-u":
            idx = idx + 1
            username = sys.argv[idx]
        elif option == "-p":
            idx = idx + 1
            password = sys.argv[idx]
        elif option == "-t":
            idx = idx + 1
            __TIMEOUT__ = int(sys.argv[idx])
        # end of if option.
        idx = idx + 1
    # end of while idx < len (sys.argv).
    idx = 0
    if username != '' and password != '':
        while idx < len(hosts):
            hosts[idx]['user'] = username
            hosts[idx]['password'] = password
            idx = idx + 1
    if len(hosts) > 0:
        clearHostsHD(hosts, action)
#    
# main function
#
if __name__ == '__main__':
    if len(sys.argv) < 2:
        print_help()
    argument_parser()
