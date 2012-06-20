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
    pass

def _check_snapshot_in_progress():
    '''Check if the tag /root/.s3ql/.snapshotting exists. If so, return true.'''

    try:
        if os.path.exists(snapshot_tag):
            return True
        return False
    except:
        raise SnapshotError("Could not decide whether a snapshot is in progress.")

def _initialize_snapshot():

    try:
        subprocess.Popen('sudo %s' % snapshot_bot, shell = True)
    except:
        raise SnapshotError("Could not initialize the snapshot bot.")

def take_snapshot():

    log.info('Started take_snapshot')
    return_result = False
    return_msg = '[2] Unexpected error in take_snapshot'

    try:
        if _check_snapshot_in_progress():
            return_msg = 'Another snapshotting process is already in progress. Aborting.'
        else:
            _initialize_snapshot()
            return_result = True
            return_msg = 'Completed take_snapshot'
    except SnapshotError as Err:
        return_msg = str(Err)

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'data': {}}
    return json.dumps(return_val)


def set_snapshot_schedule(snapshot_time):

    if snapshot_time >= 0:
        print('Snapshot time is set to %d:00' % snapshot_time)
    else:
        print('Snapshot schedule is disabled')

    return_val = {'result': True,
                  'msg': 'Done setting snapshot schedule.',
                  'data': {}}
    return json.dumps(return_val)


def get_snapshot_schedule():

    return_val = {'result': True,
                  'msg': 'Done setting snapshot schedule.',
                  'data': {'snapshot_time': 1}}
    return json.dumps(return_val)


def get_snapshot_list():

    time1 = time.time()
    time2 = time1 + 20
    time3 = time2 + 100

    #Two sample snapshot entries, one finished and one in progress
    snapshots = [{'name': 'demosnapshot', 'start_time': time1, \
                  'finish_time': time2, 'num_files': 100, \
                  'total_size': 100000, 'exposed': True},
                 {'name': 'new_snapshot', 'start_time': time3, \
                  'finish_time': -1, 'num_files': 0, \
                  'total_size': 0, 'exposed': False}]

    return_val = {'result': True,
                  'msg': 'Done getting snapshot list.',
                  'data': {'snapshots': snapshots}}
    return json.dumps(return_val)


def get_snapshot_in_progress():

    log.info('Started get_snapshot_in_progress')
    return_result = False
    return_msg = '[2] Unexpected error in get_snapshot_in_progress'
    in_progress_name = ""

    try:
        if _check_snapshot_in_progress():
            in_progress_name = "new_snapshot"
        else:
            in_progress_name = ""
        return_result = True
        return_msg = 'Completed get_snapshot_in_progress'
    except SnapshotError as Err:
        return_msg = str(Err)

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'data': {'in_progress': in_progress_name}}
    return json.dumps(return_val)


def expose_snapshot(to_expose):

    for snapshot in to_expose:
        print('Exposing snapshot (name: %s) as samba share' % snapshot)

    return_val = {'result': True,
                  'msg': 'Finished exposing snapshot.',
                  'data': {}}
    return json.dumps(return_val)


def delete_snapshot(to_delete):

    if type(to_delete) is not str:
        return_val = {'result': False,
                      'msg': 'Invalid name for snapshot to delete.',
                      'data': {}}
        return json.dumps(return_val)

    print('Deleting snapshot (name: %s)' % to_delete)

    return_val = {'result': True,
                  'msg': 'Finished deleting snapshot.',
                  'data': {}}
    return json.dumps(return_val)


def _write_snapshot_lifespan(months_to_live):
    '''Function for actually writing lifespan config to file'''

    try:
        with open(lifespan_conf, 'w') as fh:
            fh.write('%d' % months_to_live)
        os.system('sudo chown www-data:www-data %s' % lifespan_conf)
    except:
        raise SnapshotError('Unable to write snapshot lifespan config to file')


def set_snapshot_lifespan(months_to_live):

    log.info('Started set_snapshot_lifespan')
    print('Lifespan of a snapshot is set to %d months' % months_to_live)

    return_result = False
    return_msg = '[2] Unexpected error in get_snapshot_lifespan'

    if months_to_live > 0:
        try:
            _write_snapshot_lifespan(months_to_live)
            return_result = True
            return_msg = 'Completed set_snapshot_lifespan'
        except SnapshotError as Err:
            return_msg = str(Err)
    else:
        return_msg = 'Error in set_snapshot_lifespan: months_to_live must be a positive integer.'

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'data': {}}
    return json.dumps(return_val)


def get_snapshot_lifespan():

    log.info('Started get_snapshot_lifespan')
    months_to_live = 12
    return_result = False
    return_msg = '[2] Unexpected error in get_snapshot_lifespan'
    reset_config = False

    try:
        with open(lifespan_conf, 'r') as fh:
            data_in = fh.readline()
            try:
                months_to_live = int(data_in)
                return_result = True
                return_msg = 'Completed get_snapshot_lifespan'
            except:
                log.info('Stored lifespan is not an integer. Resetting to default (12 months).')
                reset_config = True
    except IOError:
        log.info('Unable to open config for snapshot lifespan. Resetting to default (12 months).')
        reset_config = True
    except:
        pass

    if reset_config:
        try:
            _write_snapshot_lifespan(12)
            months_to_live = 12
            return_result = True
            return_msg = 'Completed get_snapshot_lifespan'
        except SnapshotError as Err:
            return_msg = str(Err)

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'data': {'months_to_live': months_to_live}}
    return json.dumps(return_val)


################################################################

if __name__ == '__main__':
    pass
