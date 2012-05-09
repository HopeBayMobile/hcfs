#!/bin/bash
#Saved as change-samba-password.sh
USER=$2
PASS=$1
echo -ne $PASS'\n'$PASS |  pdbedit $USER -a -t

