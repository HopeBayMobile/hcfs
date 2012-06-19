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


class SnapshotError(Exception):
    log.info("[0] Error in taking snapshot")
    pass

def _execute_take_snapshot():

    log.info('This is a blank holder for the code')
    time.sleep(200)
    log.info('Done testing')

################################################################

if __name__ == '__main__':
    _execute_take_snapshot()
    pass
