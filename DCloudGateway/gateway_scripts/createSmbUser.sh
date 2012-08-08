if [ $# != 1 ]; then
        echo "Please enter the correct parameters!"
        echo "For example:"
        echo "sh createSmbUser.sh superuser"
        exit 1
fi

USER=$1
adduser $USER << EOF
$USER
$USER





Y
EOF
