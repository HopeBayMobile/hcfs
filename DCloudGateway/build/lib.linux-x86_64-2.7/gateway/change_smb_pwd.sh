#Saved as change-samba-password.sh
USER=$1
PASS=$2
echo $PASS'\n'$PASS |  pdbedit $USER -a -t