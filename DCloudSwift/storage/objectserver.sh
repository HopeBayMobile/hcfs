if [ $# != 1 ]; then
        echo "Please enter the correct parameters!"
        echo "For example:"
        echo "./objectserver.sh IP"
        exit 1
fi
IP=$1
export STORAGE_LOCAL_NET_IP=$IP

cat >/etc/swift/object-server.conf <<EOF
[DEFAULT]
bind_ip = $STORAGE_LOCAL_NET_IP
workers = 2

[pipeline:main]
pipeline = object-server

[app:object-server]
use = egg:swift#object

[object-replicator]

[object-updater]

[object-auditor]
EOF
