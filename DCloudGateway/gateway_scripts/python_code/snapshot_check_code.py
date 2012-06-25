import os.path
import common
import subprocess
from datetime import datetime

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

snapshot_tag = "/root/.s3ql/.snapshotting"
snapshot_bot = "/etc/delta/snapshot_bot"
snapshot_db_lock = "/root/.s3ql/.snapshot_db_lock"


class SnapshotError(Exception):
    pass


def continue_snapshot_in_progress():

    if os.path.exists(snapshot_db_lock):
        os.system('sudo rm -rf %s' % snapshot_db_lock)

    try:
        if os.path.exists(snapshot_tag):
            try:
                subprocess.Popen('sudo %s' % snapshot_bot, shell=True)
                log.info('Continue snapshotting process.')
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
        log.info('Error in startup gateway (snapshot)')
        log.info('%s' % str(err))
