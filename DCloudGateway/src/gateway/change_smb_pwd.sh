#Saved as change-samba-password.sh
USER=$1
PASS=$2

#sudo pdbedit -x -u $USER
sudo echo -ne $PASS'\n'$PASS | pdbedit $USER -a -t
