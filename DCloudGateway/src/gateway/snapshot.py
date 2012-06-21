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
snapshot_schedule = "/etc/delta/snapshot_schedule"
snapshot_db = "/root/.s3ql/snapshot_db.txt"
snapshot_db_lock = "/root/.s3ql/.snapshot_db_lock"


class SnapshotError(Exception):
    pass

class Snapshot_Db_Lock():

    def __init__(self):

        self.locked = False
        try:
            finish = False
            while not finish:
                if os.path.exists(snapshot_db_lock):
                    time.sleep(10)
                else:
                   os.system('sudo touch %s' % snapshot_db_lock)
                   finish = True
        except:
            raise SnapshotError('Unable to acquire snapshot db lock')
        self.locked = True


    def __del__(self):

        try:
            if self.locked:
                self.locked = False
                if os.path.exists(snapshot_db_lock):
                    os.system('sudo rm -rf %s' % snapshot_db_lock)
        except:
            raise SnapshotError('Unable to release snapshot sb lock')


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

    log.info('Started set_snapshot_schedule')
    return_result = False
    return_msg = '[2] Unexpected error in set_snapshot_schedule'

    try:
        if os.path.exists(snapshot_schedule):
            os.system('sudo rm -rf %s' % snapshot_schedule)

        with open(snapshot_schedule,'w') as fh:
            fh.write('%d' % snapshot_time)

        return_result = True
        return_msg = 'Completed set_snapshot_schedule'
    except:
        return_msg = '[2] Unable to write snapshot schedule'

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'data': {}}
    return json.dumps(return_val)


def get_snapshot_schedule():

    log.info('Started get_snapshot_schedule')
    return_result = False
    return_msg = '[2] Unexpected error in get_snapshot_schedule'
    snapshot_time = -1

    try:
        if not os.path.exists(snapshot_schedule):
            with open(snapshot_schedule, 'w') as fh:
                fh.write('%d' % snapshot_time)
        else:
            with open(snapshot_schedule, 'r') as fh:
                tmp_line = fh.readline()
                snapshot_time = int(tmp_line)

        return_result = True
        return_msg = 'Completed get_snapshot_schedule'

    except:
        return_msg = '[2] Unable to read snapshot schedule'

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'data': {'snapshot_time': snapshot_time}}
    return json.dumps(return_val)

def _acquire_db_list():

    db_lock = Snapshot_Db_Lock()
    db_entries = []

    try:
        if os.path.exists(snapshot_db):
            with open(snapshot_db,'r') as fh:
                db_entries = fh.readlines()
    except:
        raise SnapshotError('Unable to access snapshot database')
    finally:
        del db_lock

    return db_entries

def get_snapshot_list():

    log.info('Started get_snapshot_list')
    return_result = False
    return_msg = '[2] Unexpected error in get_snapshot_list'
    snapshots = []

    try:
        db_list = _acquire_db_list()

        for entry in db_list:
            tmp_items = entry.split(',')

            if tmp_items[5] == 'true':
                tmp_exposed = True
            else:
                tmp_exposed = False

            temp_obj = {'name': tmp_items[0], \
                        'start_time': float(tmp_items[1]), \
                        'finish_time': float(tmp_items[2]),\
                        'num_files': int(tmp_items[3]), \
                        'total_size': int(tmp_items[4]), \
                        'exposed': tmp_exposed}       
            snapshots = snapshots + [temp_obj]

        return_result = True
        return_msg = 'Finished reading snapshot list'
    except:
        return_msg = '[2] Unable to read snapshot list'

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
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
    print set_snapshot_schedule(10)
    print get_snapshot_schedule()
    print get_snapshot_list()
    pass
