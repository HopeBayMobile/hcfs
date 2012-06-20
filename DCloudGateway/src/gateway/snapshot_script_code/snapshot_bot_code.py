import os.path
import sys
import csv
import json
import os
import ConfigParser
import common
import subprocess
import time
import errno
import re
from datetime import datetime

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

smb_conf_file = "/etc/samba/smb.conf"
lifespan_conf = "/etc/delta/snapshot_lifespan"
snapshot_tag = "/root/.s3ql/.snapshotting"
snapshot_bot = "/etc/delta/snapshot_bot"
snapshot_db = "/root/.s3ql/snapshot_db.txt"
temp_folder = "/mnt/cloudgwfiles/tempsnapshot"
snapshot_statistics = "/root/.s3ql/snapshot.log"
samba_folder = "/mnt/cloudgwfiles/sambashare"


class SnapshotError(Exception):
    pass


def _check_snapshot_in_progress():
    '''Check if the snapshot tag exists. If so, return true.'''

    try:
        if os.path.exists(snapshot_tag):
            return True
        return False
    except:
        raise SnapshotError("Could not decide whether a snapshot is in progress.")

def new_database_entry():
    pass

def invalidate_entry():
    pass

def execute_take_snapshot():

    log.info('Begin snapshotting bot tasks')

    if not _check_snapshot_in_progress():
        #if we are initializing a new snapshot process
        try:
            os.system("sudo touch %s" % snapshot_tag) #Tag snapshotting
            new_database_entry()
        except SnapshotError as Err:
            err_msg = str(Err)
            log.info('[2] Unexpected error in snapshotting.')
            log.info('Error message: %s' % err_msg)
            return
    else:
        if actually_not_in_progress(): #Check if there is actually a "new_snapshot" entry in database
            recover_database()    #Fix the snapshot entry and database
#Note: The database could be updated but the snapshot directory is not renamed or locked. Remove tag after this.

    finish = False

    while not finish:
        try:
            #Check if file system is still up
            if not os.path.exists(samba_folder):
                return

            if os.path.exists(temp_folder):
                os.system("sudo rm -rf %s" % temp_folder)

            if os.path.exists(snapshot_statistics):
                os.system('sudo rm -rf %s' % snapshot_statistics)

            os.system("sudo mkdir %s" % temp_folder)
            cmd ="sudo python /usr/local/bin/s3qlcp /mnt/cloudgwfiles/sambashare %s/sambashare" % temp_folder
            po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()

            if po.returncode != 0:
                if output.find("Dirty cache has not been completely flushed") !=-1:
                    time.sleep(60) #Wait one minute before retrying
                else:
                    #Check if file system is still up
                    if not os.path.exists(samba_folder):
                        return

                    invalidate_entry()
                    log.info('[2] Unable to finish the current snapshotting process. Aborting.')
                    os.system('sudo rm -rf %s' % snapshot_tag)
                    return
            else:
                with open(snapshot_statistics, 'r') as fh:
                    for lines in fh:
                        if lines.find("total files") != -1:
                            samba_files = int(lines.lstrip('toal fies:'))
                        if lines.find("total size") != -1:
                            samba_size = int(lines.lstrip('toal size:'))
                samba_size = int(samba_size / 1000000)
                os.system('sudo rm -rf %s' % snapshot_statistics)

                cmd ="sudo python /usr/local/bin/s3qlcp /mnt/cloudgwfiles/nfsshare %s/nfsshare" % temp_folder
                po  = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                output = po.stdout.read()
                po.wait()

                if po.returncode != 0:
                    if output.find("Dirty cache has not been completely flushed") !=-1:
                        time.sleep(60) #Wait one minute before retrying
                    else:
                        #Check if file system is still up
                        if not os.path.exists(samba_folder):
                            return

                        invalidate_entry()
                        log.info('[2] Unable to finish the current snapshotting process. Aborting.')
                        os.system('sudo rm -rf %s' % snapshot_tag)
                        return
                else:
                    with open(snapshot_statistics, 'r') as fh:
                        for lines in fh:
                            if lines.find("total files") != -1:
                                nfs_files = int(lines.lstrip('toal fies:'))
                            if lines.find("total size") != -1:
                                nfs_size = int(lines.lstrip('toal size:'))
                    nfs_size = int(nfs_size / 1000000)
                    os.system('sudo rm -rf %s' % snapshot_statistics)

                    finish = True
        except Exception as err:
            #Check if file system is still up
            if not os.path.exists(samba_folder):
                return

            print err
            raise

################################################################

if __name__ == '__main__':
    execute_take_snapshot()
    pass
