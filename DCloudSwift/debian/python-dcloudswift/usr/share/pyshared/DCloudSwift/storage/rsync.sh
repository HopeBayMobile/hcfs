if [ $# != 1 ]; then
        echo "Please enter the correct parameters!"
        echo "For example:"
        echo "./rsync.sh IP"
        exit 1
fi


IP=$1
export STORAGE_LOCAL_NET_IP=$IP


perl -pi -e 's/RSYNC_ENABLE=false/RSYNC_ENABLE=true/' /etc/default/rsync

cat >/etc/rsyncd.conf <<EOF
uid = swift
gid = swift
log file = /var/log/rsyncd.log
pid file = /var/run/rsyncd.pid
address = $STORAGE_LOCAL_NET_IP

[account]
max connections = 2
path = /srv/node/
read only = false
lock file = /var/lock/account.lock

[container]
max connections = 2
path = /srv/node/
read only = false
lock file = /var/lock/container.lock

[object]
max connections = 2
path = /srv/node/
read only = false
lock file = /var/lock/object.lock
EOF
