#!/usr/bin/env python
'''
s3ql_tx_uploader.py

Copyright (C) Delta Cloud Technology BD.

This program upload s3ql transaction logs in Savebox.
'''
import os
import time
import cPickle
from Crypto.Cipher import AES
from Crypto import Random
from Crypto.Util import Counter
from Crypto.Hash import HMAC
from Crypto.Hash import SHA
from Crypto.Hash import SHA256
from gateway import api

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
   
def encrypt_string_aes128(key, plaintext):
    '''
    Encrypt the plaintext using the key k
    AES-128 CTR mode is used and also with a MAC 
    @param: key
    @param: plaintext
    @return (iv, ciphertext, MAC)
    '''

    if len(key) != KEYSIZE:
        raise ValueError('key length should be %d bytes long' % KEYSIZE)
    
    #produce IV
    random = Random.new()
    iv = random.read( BLOCKSIZE )
    random.close()
    ctr = Counter.new( BLOCKSIZEBITS, initial_value = int( iv.encode('hex'), 16 ) )
    #AES-CTR
    cipher = AES.new( key, AES.MODE_CTR, counter = ctr )
    ciphertext = cipher.encrypt(plaintext)
    #HMAC-SHA
    hmac = HMAC.new(key, digestmod=SHA)
    hmac.update(iv+ciphertext)
    return (iv, ciphertext, hmac.digest())


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

def do_upload(postfix):
    """
    Upload the encrypted file to [User]_gateway_config container
    @param: timestamp as the postfix of file name which would be uploaded. 
    """
    try:
        storage_url, account, password = api._get_storage_info()
        if storage_url and account and password:
            # file prefix we want to filter
            file_path = "/root/.s3ql/s3ql_txlog/"
            file_postfix = postfix
            file_name = "s3ql_tx_" + file_postfix
            _, username = account.split(':')
            os.chdir(file_path)

            cmd = 'sudo gzip %s' % (file_name)
            os.system(cmd)

            command = 'sudo swift -A https://%s/auth/v1.0 -U %s -K %s upload %s_private_container %s.gz' \
                    % (storage_url, account, password, username, file_name)
            _, output = api._run_subprocess(command, 300) 
    except Exception as e:
        print(str(e))
    
def start_upload(full_upload=0):
    """
    Encrypt tx_log files and do uploading.
    If full_upload is set, upload the whole day logs.
    @param: full_upload
    """    
    src_path = "/root/.s3ql/s3ql_txlog/s3ql_txlog."
    timestamp = int(time.time())

    if full_upload:
        src_path = src_path + "day"
        file_postfix = str(timestamp) + ".full"
    else: 
        src_path = src_path + "hour"
        file_postfix = str(timestamp) + ".partial"

    with open(src_path, "r") as src:
        cipher = encrypt_string_aes128(get_key(), src.read())

    dest_path = "/root/.s3ql/s3ql_txlog/s3ql_tx_" + file_postfix
    with open(dest_path, "w") as dest:
        cPickle.dump(cipher, dest, -1)

    do_upload(file_postfix)

    cmd = 'sudo rm %s.gz' % (dest_path)
    os.system(cmd)

def _upload():
    """
    To upload tx_log files to cloud.
    """
    os.system("sudo touch /dev/shm/gw_upload_txlog")
    
    while os.path.exists("/dev/shm/gw_upload_txlog"):
        time.sleep(1)

    if time.strftime('%H') == '00':
        if os.path.exists("/root/.s3ql/s3ql_txlog/s3ql_txlog.hour"):
            os.system("sudo cat /root/.s3ql/s3ql_txlog/s3ql_txlog.hour >> /root/.s3ql/s3ql_txlog/s3ql_txlog.day")
        if os.path.exists("/root/.s3ql/s3ql_txlog/s3ql_txlog.day"):
            start_upload(full_upload=1)
        os.system("sudo rm -f /root/.s3ql/s3ql_txlog/*")
    else:
        if os.path.exists("/root/.s3ql/s3ql_txlog/s3ql_txlog.hour"):
            os.system("sudo cat /root/.s3ql/s3ql_txlog/s3ql_txlog.hour >> /root/.s3ql/s3ql_txlog/s3ql_txlog.day")
            start_upload()
            os.system("sudo rm -f /root/.s3ql/s3ql_txlog/s3ql_txlog.hour")

#TODO: encrypt/decrypt large file

if __name__ == "__main__":
    _upload()
