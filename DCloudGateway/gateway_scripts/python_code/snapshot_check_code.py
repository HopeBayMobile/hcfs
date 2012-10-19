"""
This script is part of Delta Cloud Storage Gateway API functions
Developed by CTBU, Delta Electronics Inc., 2012

This script checks for unfinished snapshotting at system start
and initiate continuation of the process if one exits.
"""
import os.path
import subprocess
from datetime import datetime
from gateway import common

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

snapshot_tag = "/root/.s3ql/.snapshotting"
snapshot_bot = "/etc/delta/snapshot_bot"
snapshot_db_lock = "/root/.s3ql/.snapshot_db_lock"


class SnapshotError(Exception):
    pass


def continue_snapshot_in_progress():
    """
    Continue a in-progress snapshotting at system startup.

    Function also cleans-up database lock file.
    """
    if os.path.exists(snapshot_db_lock):
        os.system('sudo rm -rf %s' % snapshot_db_lock)

    try:
        if os.path.exists(snapshot_tag):
            try:
                subprocess.Popen('sudo %s' % snapshot_bot, shell=True)
                log.debug('Continue snapshotting process.')
            except:
                raise SnapshotError("Could not initialize the snapshot bot.")

        return
    except:
        raise\
          SnapshotError("Could not decide whether a snapshot is in progress.")


################################################################

if __name__ == '__main__':
    try:
        continue_snapshot_in_progress()
    except Exception as err:
        log.error('Error in startup gateway (snapshot)')
        log.error('%s' % str(err))
