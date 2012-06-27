"""
This script is part of Delta Cloud Storage Gateway API functions
Developed by CTBU, Delta Electronics Inc., 2012

This script initiates scheduled snapshotting.
"""
import os.path
import common
import subprocess
import time
from datetime import datetime

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

snapshot_tag = "/root/.s3ql/.snapshotting"
snapshot_bot = "/etc/delta/snapshot_bot"
snapshot_db_lock = "/root/.s3ql/.snapshot_db_lock"
snapshot_schedule = "/etc/delta/snapshot_schedule"


class SnapshotError(Exception):
    pass


def check_snapshot_schedule():
    """
    Check the current snapshot schedule.
    """
    log.info('Loading snapshot schedule')

    snapshot_time = -1

    try:
        if os.path.exists(snapshot_schedule):
            with open(snapshot_schedule, 'r') as fh:
                tmp_line = fh.readline()
                snapshot_time = int(tmp_line)

        log.info('Completed loading snapshot schedule')

    except:
        raise SnapshotError('Unable to load snapshot schedule')

    return snapshot_time


def take_scheduled_snapshot():
    """
    Call snapshotting bot to take scheduled snapshots.
    """
    try:
        if not os.path.exists(snapshot_tag):
            try:
                subprocess.Popen('sudo %s' % snapshot_bot, shell=True)
                log.info('Taking scheduled snapshot (bot started).')
            except:
                raise SnapshotError("Could not initialize the snapshot bot.")

        return
    except:
        raise\
          SnapshotError("Could not decide whether a snapshot is in progress.")


################################################################

if __name__ == '__main__':
    try:
        snapshot_time = check_snapshot_schedule()
        current_time = time.localtime()
        current_hour = current_time.tm_hour
        if current_hour == snapshot_time:
            take_scheduled_snapshot()
    except Exception as err:
        log.info('Error in hourly snapshotting check')
        log.info('%s' % str(err))
