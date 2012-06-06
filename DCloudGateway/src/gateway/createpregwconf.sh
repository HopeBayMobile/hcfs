#if [ $# != 1 ]; then
#        echo "Please enter the correct parameters!"
#        echo "For example:"
#        echo "sh createS3qlConf.sh "
#        exit 1
#fi


URL=$1


cat > /etc/init/pre-gwstart.conf <<EOF
description	"upstart script for checking for health of network and then bring up the gateway"
author		"Jiahong Wu <jiahong.wu@delta.com.tw"

start on (filesystem and net-device-up)

env STORAGE_ADDR="$URL"

script
    su -s /bin/sh -c 'exec "\$0" "\$@"' "root" -- \\
        /etc/delta/wait_network_up "\$STORAGE_ADDR"
end script
EOF
