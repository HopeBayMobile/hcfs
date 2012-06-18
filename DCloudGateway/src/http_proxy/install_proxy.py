#!/usr/bin/python

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.

# Function: Install and config squid3 on a gateway
# needs to be runned as root

import os

cache_dir = "/storage/http_proxy_cache/"

### 1. install Squid3 package
cmd = "apt-get install squid3"
os.system(cmd)

### 2. setup config file
os.system("mkdir -p " + cache_dir)
os.system("bash gen_squid3_conf.sh " + cache_dir)
os.system("chmod 777 " + cache_dir)
print "Squid3 configuration has been written."

### 3. Restart Squid3 service
os.system("service squid3 restart")
