#!/usr/bin/env python
'''
s3ql_tx_rotator.py

Copyright (C) Delta Cloud Technology BD.

The program rotates s3ql transaction logs in cloud storage by pre-defined days.
'''
from gateway import api
from gateway import common
import time

log = common.getLogger(name="S3QL_TX_ROTATOR", conf="/etc/delta/Gateway.ini")

# log duration 60 days
LOG_DURATION = 60 * 24 * 60 * 60

def main():
    try:
        # get storage url/account/password from authinfo2
        storage_url, account, password = api._get_storage_info()
        if storage_url and account and password:
            # file prefix we want to filter
            file_prefix = "s3ql_tx"
            _, username = account.split(':')
            command = 'sudo swift -A https://%s/auth/v1.0 -U %s -K %s list %s_gateway_config -p %s' \
                    % (storage_url, account, password, username, file_prefix)
            _, output = api._run_subprocess(command, 15)
            
            # outdated file set
            outdated_files = set()
            files = output.split('\n')
            
            # sync local time to ntp before getting current time
            api._run_subprocess('sudo /etc/delta/synctime.sh')
            # get current time
            current_time = int(time.time())
            
            # iterate file list and look for outdated logs
            for file in files:
                try:
                    # format s3ql_tx_[TIMESTAMP].gz
                    file_time = int(file.split('.')[0].split('_')[2])
                    if (current_time - file_time) > LOG_DURATION:
                        outdated_files.add(file)
                # last line of swift cli output is empty, just catch and ignore it
                except IndexError:
                    pass
            
            # iterate outdated files and call swift cli to delete them
            for file in outdated_files:
                command = 'sudo swift -A https://%s/auth/v1.0 -U %s -K %s delete %s_gateway_config %s' \
                    % (storage_url, account, password, username, file)
                _, output = api._run_subprocess(command, 15)
                if output.find(file) != -1:
                    # swift should successfully delete that file
                    log.debug("Deleted outdated S3QL transaction log: %s" % file)
                else:
                    # failed to delete that file. for now just log it
                    log.debug("Deleteing outdated S3QL transaction log failed. (%s)" % file)
    except Exception as e:
        log.debug(str(e))

if __name__ == '__main__':
    main()
