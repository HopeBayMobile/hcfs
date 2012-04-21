if [ $# != 1 ]; then
        echo "Please enter the correct parameters!"
        echo "For example:"
        echo "./Containerserver IP"
        exit 1
fi


IP=$1
export PROXY_LOCAL_NET_IP=$IP
export STORAGE_LOCAL_NET_IP=$IP

cat >/etc/swift/container-server.conf <<EOF
[DEFAULT]
bind_ip = $STORAGE_LOCAL_NET_IP
workers = 2

[pipeline:main]
pipeline = container-server

[app:container-server]
use = egg:swift#container

[container-replicator]

[container-updater]

[container-auditor]
EOF
