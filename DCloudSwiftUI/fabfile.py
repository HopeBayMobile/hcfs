"""
web deploy script
0.1 fredlin 2011/9/3
"""
from fabric.api import *

#modify local repository path
src_path = "/home/swift/StorageAppliance/DCloudSwiftUI/swiftUI/"

env.hosts=["10.1.0.17"]
env.user="root"
env.password="deltacloud"

def setup():
    #setup route for local linux machine
    local("sudo route add -net 10.1.0.0 netmask 255.255.0.0 gw 172.16.78.252")

def vncserver():
    #setup vnc tunnel, the console will jump to the target server
    run("ssh -L 5901:192.168.11.22:5901 root@192.168.11.22")

def vnc():
    #start vncviewer on local machine
    local("vncviewer -via root@"+env.hosts[0]+" :1")

def deploy():
    # deploy local web directory to target server
    local("scp -r "+src_path+" root@10.1.0.17:/templateVol/2/rw/TPE1AA0122/var/www-zcw/")
