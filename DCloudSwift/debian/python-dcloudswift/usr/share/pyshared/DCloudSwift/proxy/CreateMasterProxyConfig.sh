if [ $# != 3 ]; then
        echo "Please enter the correct parameters!"
        echo "For example:"
        echo "./CreateProxyConfig BindPort ProxyIP PortalUrl"
        exit 1
fi



export BIND_PORT=$1
export PROXY_LOCAL_NET_IP=$2
export PORTAL_URL=$3

mkdir -p /etc/swift
chown -R swift:swift /etc/swift/
cd /etc/swift


cat >/etc/swift/swift.conf <<EOF
[swift-hash]
# random unique string that can never change (DO NOT LOSE)
swift_hash_path_suffix =  69b4da4fbde33158
EOF


openssl req -new -x509 -nodes -out cert.crt -keyout cert.key << EOF
TW
Taiwan
Taipei
Delta
CTC
CW
cw.luo@delta.com.tw
EOF


perl -pi -e "s/-l *.*.*.*/-l $PROXY_LOCAL_NET_IP/" /etc/memcached.conf

cat >/etc/swift/proxy-server.conf <<EOF
[DEFAULT]
cert_file = /etc/swift/cert.crt
key_file = /etc/swift/cert.key
bind_port = $BIND_PORT
workers = 8
user = swift

[pipeline:main]
pipeline = healthcheck cache swauth proxy-server

[app:proxy-server]
use = egg:swift#proxy
allow_account_management = true

[filter:swauth]
use = egg:swauth#swauth
set log_name = root
default_swift_cluster = local#$PORTAL_URL/v1#https://127.0.0.1:$BIND_PORT/v1
super_admin_key = deltacloud

[filter:healthcheck]
use = egg:swift#healthcheck

[filter:cache]
use = egg:swift#memcache
memcache_servers = $PROXY_LOCAL_NET_IP:11211
EOF

