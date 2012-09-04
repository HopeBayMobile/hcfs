"""
web deploy script
0.2 fredlin 2012/9/4

Install
-----------------

pre-requisite package:

$ pip install fabric


2 Step Usage:
------------------

1. First time usage:

In linux machine, run command in console:

$ fab setup
$ fab vncserver

2. then open another console to run

$ fab vnc

Deploy Web
------------------

edit src_path to reflect your local path.

run command:

$ fab deploy

to update web ui.

Deploy Module
------------------

edit src_path to reflect your local path.

run command:

$ fab deploy_module

to update module.

Reload Target web Server
--------------------------

(this command does not included in this tool)

In target server you may need to run

# /etc/init.d/apache-zcw reload

to reload web.

"""
from fabric.api import *

#modify local repository path
src_path = "~/StorageAppliance/DCloudSwiftUI/swiftUI/"
module_path = "~/StorageAppliance/DCloudSwift/src/DCloudSwift/"

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

def deploy_module():
    local("scp -r "+module_path+" root@10.1.0.17:/templateVol/2/rw/TPE1AA0122/usr/local/lib/python2.7/dist-packages/DCloudSwift-0.5-py2.7.egg/")

def deploy():
    # deploy local web directory to target server
    local("scp -r "+src_path+" root@10.1.0.17:/templateVol/2/rw/TPE1AA0122/var/www-zcw/")
