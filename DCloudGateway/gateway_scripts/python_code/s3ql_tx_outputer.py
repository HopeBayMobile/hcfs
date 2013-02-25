#!/usr/bin/env python
'''
s3ql_tx_uploader.py

Copyright (C) Delta Cloud Technology BD.

This program upload s3ql transaction logs in Savebox.
'''
import os
import time
import cPickle
import subprocess
import json
import getpass
from Crypto.Cipher import AES
from Crypto import Random
from Crypto.Util import Counter
from Crypto.Hash import HMAC
from Crypto.Hash import SHA
from Crypto.Hash import SHA256


storage_url = ""
storage_account = ""
storage_user = ""
storage_passwd = ""

KEYSIZE = 16
BLOCKSIZE = AES.block_size
BLOCKSIZEBITS = BLOCKSIZE * 8
MACSIZE = 20


def get_key():
    '''
    to get the aes128 key
    @return key
    '''

    key = "t2a5l6oUd"
    h = SHA256.new()
    h.update(key)
    aeskey = h.digest()

    return aeskey[:16]

def check_swift_ok():

    cmd = "swift -A https://%s/auth/v1.0 -U %s:%s -K %s stat %s_private_container" \
           % (storage_url, storage_account, storage_user, storage_passwd, \
           storage_user )

    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()

    if output.find("Bytes:") != -1:
        swift_ok = True
    else:
        swift_ok = False

    return swift_ok

def get_storage_info():

    global storage_url
    global storage_account
    global storage_user
    global storage_passwd

    print("type storage infos - ")
    storage_url = raw_input('url:')
    storage_account = raw_input('account:')
    storage_user = raw_input('user:')
    storage_passwd = getpass.getpass()


def decrypt_string_aes128(key, iv, ciphertext, mac):
    '''
    Decrypt the ciphertext using the key k
    AES-128 CTR mode is used and also with a MAC
    @param: key
    @param: iv 
    @param: ciphertext
    @param: mac
    @return plaintext if decryption success else None
    '''

    decrypt_flag = True
    #check the size of key and the size of iv
    if len(key) != KEYSIZE or len(iv) != BLOCKSIZE:
        key = Random.new().read( KEYSIZE )
        iv = Random.new().read( BLOCKSIZE )
        decrypt_flag = False

    #HMAC-SHA
    hmac = HMAC.new(key, digestmod=SHA)
    hmac.update(iv+ciphertext)
    testmac = hmac.digest()
    #Test if testmac == mac
    sha = SHA.new()
    sha.update(testmac)
    hashtestmac = sha.digest()

    sha = SHA.new()
    sha.update(mac)
    hashmac = sha.digest()

    if hashtestmac != hashmac:
        decrypt_flag = False
    #AES-CTR
    ctr = Counter.new( BLOCKSIZEBITS, initial_value = int( iv.encode('hex'), 16 ) )
    cipher = AES.new( key, AES.MODE_CTR, counter = ctr )
    plaintext = cipher.decrypt(ciphertext)

    return plaintext if decrypt_flag else None

def list_file():
    
    cmd = "swift -A https://%s/auth/v1.0 -U %s:%s -K %s list %s_private_container -p s3ql \
           | grep full" % (storage_url, storage_account, storage_user, storage_passwd, \
                           storage_user )

    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait() 

    file_list = output.split("\n")
    del file_list[-1]
    
    # the time of last full backup
    if len(file_list) == 0:
        last_full_backup = 0
    else:
        last_full_backup = file_list[-1].split("_")[2].split(".")[0]
    # we also need to parse additional s3ql_tx_[timestamp].partial.gz files 
    # since we do not have a full backup yet today  
    cmd = "swift -A https://%s/auth/v1.0 -U %s:%s -K %s list %s_private_container -p s3ql \
           | grep partial | tail -23 " % (storage_url, storage_account, storage_user, storage_passwd, \
                                      storage_user )

    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()

    file_list_partial = output.split("\n")
    del file_list_partial[-1]

    for txlog in file_list_partial:
        backup_time = txlog.split("_")[2].split(".")[0]
        if backup_time > last_full_backup:
            file_list.append(txlog)

    return file_list    

def main():

    get_storage_info()

    if not check_swift_ok():
        print("Get transction log failed: can not connect to backend")
        return

    file_list = list_file()
    if len(file_list) == 0:
        print("Get transction log failed: no transction logs in cloud")
        return

    os.system("mkdir s3ql_tx_report")
    os.chdir("s3ql_tx_report")

    for tx_log in file_list:
        
        cmd = "sudo swift -A https://%s/auth/v1.0 -U %s:%s -K %s download %s_private_container %s \
               | grep full" % (storage_url, storage_account, storage_user, storage_passwd, \
                               storage_user, tx_log )

        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()

        cmd = "gunzip %s" % (tx_log)
        os.system(cmd)

        with open(tx_log[:-3],"r") as fh:
            cp = cPickle.loads(fh.read())

        with open(tx_log[:-3],"w") as fh:
            fh.write( decrypt_string_aes128(get_key(), cp[0], cp[1], cp[2]))
    
    os.system("cat * > raw_data.output")

    with open("raw_data.output", "r") as fh:
        tx_logs = fh.readlines()
        
    with open("final_report.output","a") as fh:
        fh.write("date,time,operation,filename,inode,blockno\n")
        for lines in tx_logs:
            tx_log = json.loads(lines)
            record_time = time.strftime("%Y%m%d,%H:%M:%S", time.localtime(tx_log['timestamp']))
            fh.write("%s,%s,%s,%s,%s\n" % (record_time, tx_log['op'], tx_log['name'], tx_log['inode'], tx_log['blockno']))
    

 
if __name__ == "__main__":
    main()
