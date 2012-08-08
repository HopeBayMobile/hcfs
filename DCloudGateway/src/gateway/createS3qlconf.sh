#if [ $# != 1 ]; then
#        echo "Please enter the correct parameters!"
#        echo "For example:"
#        echo "sh createS3qlConf.sh "
#        exit 1
#fi


IFACE=$1
URL=$2
MOUNTPOINT=$3
MOUNTOPT=$4


cat > /etc/init/s3ql.conf <<EOF
description	"S3QL Backup File System"
author		"Nikolaus Rath <Nikolaus@rath.org>"

# This assumes that eth0 provides your internet connection
start on (filesystem and net-device-up IFACE=$IFACE and stopped pre-gwstart)

# We can't use "stop on runlevel [016]" because from that point on we
# have only 10 seconds until the system shuts down completely.
stop on starting rc RUNLEVEL=[016]

# Time to wait before sending SIGKILL to the daemon and
# pre-stop script
kill timeout 30

env STORAGE_URL="$URL"
env MOUNTPOINT="$MOUNTPOINT"
env AUTHFILE="/root/.s3ql/authinfo2"
env CACHEDIR="/root/.s3ql"

expect stop

script
    # Redirect stdout and stderr into the system log
    DIR=\$(mktemp -d)
    mkfifo "\$DIR/LOG_FIFO"
    logger -t s3ql -p local0.info < "\$DIR/LOG_FIFO" &
    exec > "\$DIR/LOG_FIFO"
    exec 2>&1
    rm -rf "\$DIR"

    # Check and mount file system
    su -s /bin/sh -c 'exec "\$0" "\$@"' "root" -- \\
        fsck.s3ql --batch --authfile "\$AUTHFILE" --cachedir "\$CACHEDIR" "\$STORAGE_URL"
    exec su -s /bin/sh -c 'exec "\$0" "\$@"' "root" -- \\
        mount.s3ql $MOUNTOPT --upstart --authfile "\$AUTHFILE" --cachedir "\$CACHEDIR" "\$STORAGE_URL" "\$MOUNTPOINT"
end script

pre-stop script
    su -s /bin/sh -c 'exec "\$0" "\$@"' "root" -- umount.s3ql "\$MOUNTPOINT"
end script
EOF
