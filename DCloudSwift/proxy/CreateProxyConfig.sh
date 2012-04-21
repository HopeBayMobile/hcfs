if [ $# != 1 ]; then
        echo "Please enter the correct parameters!"
        echo "For example:"
        echo "./CreateProxyConfig ProxyIP"
        exit 1
fi


IP=$1
export PROXY_LOCAL_NET_IP=$IP

mkdir -p /etc/swift
chown -R swift:swift /etc/swift/


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
mv cert.key /etc/swift
mv cert.crt /etc/swift


perl -pi -e "s/-l 127.0.0.1/-l $PROXY_LOCAL_NET_IP/" /etc/memcached.conf
service memcached restart

cat >/etc/swift/proxy-server.conf <<EOF
[DEFAULT]
cert_file = /etc/swift/cert.crt
key_file = /etc/swift/cert.key
bind_port = 8080
workers = 8
user = swift

[pipeline:main]
pipeline = healthcheck cache tempauth proxy-server

[app:proxy-server]
use = egg:swift#proxy
allow_account_management = true
account_autocreate = true

[filter:tempauth]
use = egg:swift#tempauth
user_system_root = testpass .admin https://$PROXY_LOCAL_NET_IP:8080/v1/AUTH_system

[filter:healthcheck]
use = egg:swift#healthcheck

[filter:cache]
use = egg:swift#memcache
memcache_servers = $PROXY_LOCAL_NET_IP:11211
EOF

