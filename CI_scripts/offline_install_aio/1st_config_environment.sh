#!/bin/bash

echo "***************************"
echo "if apt-get needs proxy, remembere to edit /etc/apt/apt.conf"
echo "E.g. add '  Acquire::http { Proxy \"http://172.16.78.253:3128\"; };  '"
echo "***************************"
echo "Acquire::http { Proxy \"http://172.16.78.253:3128\"; };" > /etc/apt/apt.conf.d/09proxy

apt-get update
apt-get install -y git dpkg-dev wput
apt-get -y install python-software-properties devscripts

# to set up proxy for git
echo "***************************"
echo "configure git to use proxy. E.g."
echo "sudo git config --global http.proxy http://<delta-user>:<delta-pass>@172.16.64.60:8080"
echo "***************************"



