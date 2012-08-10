"""
This function is part of Delta Cloud Storage Gateway API functions
Developed by CTBU, Delta Electronics Inc., 2012

This source code implements the API functions for the snapshotting
features of Delta Cloud Storage Gateway.
"""
import os.path
import json
import common
import subprocess
import time
from datetime import datetime

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

####### Global variable definition ###############

smb_conf_file = "/etc/samba/smb.conf"
org_smb_conf = "/etc/delta/smb.orig"
tmp_smb_dir = "/root/.s3ql/.tmpsmb"
tmp_smb_conf = "/root/.s3ql/.tmpsmb/.smb.conf.tmp"
tmp_smb_conf1 = "/root/.s3ql/.tmpsmb/.smb.conf.tmp1"
tmp_smb_conf2 = "/root/.s3ql/.tmpsmb/.smb.conf.tmp2"
lifespan_conf = "/etc/delta/snapshot_lifespan"
snapshot_tag = "/root/.s3ql/.snapshotting"
snapshot_bot = "/etc/delta/snapshot_bot"
snapshot_schedule = "/etc/delta/snapshot_schedule"
snapshot_db = "/root/.s3ql/snapshot_db.txt"
snapshot_db_lock = "/root/.s3ql/.snapshot_db_lock"
snapshot_dir = "/mnt/cloudgwfiles/snapshots"
temp_snapshot_db = "/root/.s3ql/.tempsnapshotdb"
temp_snapshot_db1 = "/root/.s3ql/.tempsnapshotdb1"
ss_auto_exposed_file = "/root/.s3ql/.ss_auto_exposed"

####### Exception class definition ###############


class SnapshotError(Exception):
    pass


####### Start of API function ####################


class Snapshot_Db_Lock():
    """
    Class for handling acquiring/releasing lock for snapshotting database.

    @cvar locked: Whether this class instance acquired the database lock.
    @type locked: Boolean value

    Usage:
      1. Acquiring database lock: I{lock = Snapshot_Db_Lock()}.
      2. Releasing database lock: I{del lock}.
    """
    def __init__(self):
        """
        Constructor for Snapshot_Db_Lock.

        The file defined by I{snapshot_db_lock} variable is used as the
        lock. Existance of the file infers that the database is locked.
        """
        self.locked = False
        try:
            finish = False
            while not finish:
                if os.path.exists(snapshot_db_lock):
                    time.sleep(0.5)
                else:
                    os.system('sudo touch %s' % snapshot_db_lock)
                    finish = True
        except:
            raise SnapshotError('Unable to acquire snapshot db lock')
        self.locked = True

    def __del__(self):
        """
        Destructor for Snapshot_Db_Lock.

        Deletes the lock file if it is created by this instance of
        Snapshot_Db_Lock.
        """
        try:
            if self.locked:
                self.locked = False
                if os.path.exists(snapshot_db_lock):
                    os.system('sudo rm -rf %s' % snapshot_db_lock)
        except:
            raise SnapshotError('Unable to release snapshot sb lock')


def _check_snapshot_in_progress():
    """
    Checks if there is a snapshotting process in progress.
    A tag file (defined by I{snapshot_tag}) indicates the existance
    of such a process.

    @rtype:    Boolean value
    @return:   Whether the tag for snapshotting process exists
    """
    try:
        if os.path.exists(snapshot_tag):
            return True
        return False
    except:
        raise SnapshotError("Could not decide whether a snapshot is in progress.")


def _initialize_snapshot():
    """
    Starts snapshotting bot (which actually handles the snapshotting).

    @return:   None
    @rtype:    N/A
    """
    try:
        subprocess.Popen('sudo %s' % snapshot_bot, shell=True)
    except:
        raise SnapshotError("Could not initialize the snapshot bot.")


def _wait_snapshot(old_len):
    """
    Wait for the new entry to appear in the database.

    @type old_len: integer
    @param old_len: Number of entries in database before taking snapshot
    """

    finished = False
    retries = 20
    snapshots = []

    # Wait for the snapshotting process to finish
    while not finished:
        retries = retries - 1
        db_list = _acquire_db_list()
        snapshots = _translate_db(db_list)

        new_len = len(snapshots)

        # except for checking the length of snapshots,
        # we check the snapshot name is starting with 'snapshot' as well
        if new_len > old_len:
            latest_snapshot = snapshots[0]
            finished = True
        else:
            time.sleep(0.5)


def take_snapshot():
    """
    API function for taking snapshots manually.
    
    Note that this function just inform that the action of taking snapshot is performing.
    Return from this function is not equal to the finish of taking snapshot.

    @rtype:    Json object
    @return:   A json object with function result and returned message.
    """
    log.info('Started take_snapshot')
    return_result = False
    return_msg = '[2] Unexpected error in take_snapshot'

    try:
        if _check_snapshot_in_progress():
            return_msg = 'Another snapshotting process is already in progress. Aborting.'
        else:
            db_list = _acquire_db_list()
            snapshots = _translate_db(db_list)
            old_len = len(snapshots)

            _initialize_snapshot()
            _wait_snapshot(old_len)
#            latest_ss_name = _wait_snapshot(old_len)
#            log.info('[2] Latest snapshot name: %s' % latest_ss_name)
#            # wthung, 2012/7/17
#            # automatically expose this new snapshot
#            expose_snapshot(latest_ss_name, True)
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
    """
    API function for setting the snapshot schedule.

    If the input parameter is equal to -1, the snapshot scheduling
    is disabled.

    @param snapshot_time:  Time in a day for taking snapshots
    @type snapshot_time:   Integer (from -1 to 23)
    @rtype:    Json object
    @return:   A json object with function result and returned message.
    """
    log.info('Started set_snapshot_schedule')
    return_result = False
    return_msg = '[2] Unexpected error in set_snapshot_schedule'

    try:
        if os.path.exists(snapshot_schedule):
            os.system('sudo rm -rf %s' % snapshot_schedule)

        with open(snapshot_schedule, 'w') as fh:
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
    """
    API function for getting the snapshot schedule.

    If the value of the snapshot schedule is -1, the snapshot scheduling
    is disabled.

    If there is no I{snapshot_schedule} file, the value 1 is returned
    as the snapshot schedule and this value is also written as the current
    schedule.

    @rtype:    Json object
    @return:   A json object with function result, returned message,
               and the current snapshot schedule.
    """
    log.info('Started get_snapshot_schedule')
    return_result = False
    return_msg = '[2] Unexpected error in get_snapshot_schedule'
    snapshot_time = 1

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


def get_snapshot_auto_exposed():
    """
    Get the status of snapshot auto exposed feature.
    
    @rtype: boolean
    @return: True if auto exposed feature is enabled. Otherwise, false.
    """
    
    if os.path.exists(ss_auto_exposed_file):
        return True
    return False

def set_snapshot_auto_exposed(enabled=True):
    """
    Set the status of snapshot auto exposed feature.
    
    @type enabled: boolean
    @param enabled: Status of snapshot auto exposed feature.
    @rtype: N/A
    @return: N/A
    """

    if enabled:
        os.system('sudo touch %s' % ss_auto_exposed_file)
    else:
        os.system('sudo rm -rf %s' % ss_auto_exposed_file)

def _acquire_db_list():
    """
    Helper function for reading snapshot database without parsing
    database entries.

    @rtype:    Array of strings
    @return:   Lines in the snapshot database (as array of strings).
    """
    db_lock = Snapshot_Db_Lock()
    db_entries = []

    try:
        if os.path.exists(snapshot_db):
            with open(snapshot_db, 'r') as fh:
                db_entries = fh.readlines()
    except:
        raise SnapshotError('Unable to access snapshot database')
    finally:
        del db_lock

    return db_entries


def _translate_db(db_list):
    """
    Helper function for parsing database entries into array of
    python dictionaries for snapshot entries.

    Variables in each database entry:
      1. "name": Name of the snapshot.
      2. "start_time": When the snapshot request is started.
      3. "finish_time": When the snapshot request is completed.
      4. "num_files": Total number of files included in this snapshot.
      5. "total_size": Total data size include in this snapshot.
      6. "exposed": Whether this snapshot is exposed as a samba share.
      7. "auto_exposed": Whether this snapshot is auto exposed as a samba service.

    @type db_list:  Array of strings
    @param db_list: Lines in the snapshot database (as array of strings).
    @rtype:         Array of python dictionaries
    @return:        Entries in the snapshot database.
    """
    snapshots = []
    try:
        for entry in db_list:
            tmp_items = entry.split(',')

            if tmp_items[5] == 'true':
                tmp_exposed = True
            else:
                tmp_exposed = False
            
            # wthung, 2012/7/17
            # add for auto_exposed feature
            if tmp_items[6] == 'true\n':
                tmp_auto_exposed = True
            else:
                tmp_auto_exposed = False

            temp_obj = {'name': tmp_items[0], \
                        'start_time': float(tmp_items[1]), \
                        'finish_time': float(tmp_items[2]),\
                        'num_files': int(tmp_items[3]), \
                        'total_size': int(tmp_items[4]), \
                        'exposed': tmp_exposed, \
                        'auto_exposed': tmp_auto_exposed}
            snapshots = snapshots + [temp_obj]
    except:
        raise SnapshotError('Unable to convert snapshot db')
    return snapshots


def get_snapshot_list():
    """
    API function for reading the snapshot database.

    @rtype:    Json object
    @return:   A json object with function result, returned message,
               and the current snapshot database entries.
    """
    log.info('Started get_snapshot_list')
    return_result = False
    return_msg = '[2] Unexpected error in get_snapshot_list'
    snapshots = []

    try:
        db_list = _acquire_db_list()
        snapshots = _translate_db(db_list)

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
    """
    API function for probing if there is a snapshot in progress.

    The function checks for the existance of the snapshot tag file.

    Returned value I{in_progress} is the empty string "" if there
    is no snapshot process in progress.

    @rtype:    Json object
    @return:   A json object with function result, returned message,
               and the current snapshot in progress (if any).
    """
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


# wthung, 2012/7/17
def get_snapshot_last_status():
    """
    Get status of latest snapshot.
    
    @rtype:    Json object
    @return:   A json object with function result, returned message,
               and the finish time of latest snapshot (return -1 if latest snapshot is failed).
    """

    return_msg = 'Latest snapshot is failed'
    return_result = False
    last_ss_time = -1
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'latest_snapshot_time': last_ss_time}
    
    try:
        db_list = _acquire_db_list()
        snapshot_list = _translate_db(db_list)
        
        if not snapshot_list:
            # no snapshot created
            return_result = True
            return_msg = 'No snapshot created'
            last_snapshot_time = -1
        else:
            last_ss = snapshot_list[0]
            last_ss_time = last_ss['finish_time']
            if last_ss_time > 0:
                # should be success
                return_result = True
                return_msg = 'Latest snapshot is successfully finished.'
    
    except:
        return_msg = 'Unable to get status of last snapshot.'
        last_ss_time = -1

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'latest_snapshot_time': int(last_ss_time)}
    
    return json.dumps(return_val)


def _append_samba_entry(entry):
    """
    Append new entry to a list of new exposed samba shared folders.

    @type entry:  Snapshot database entry
    @param entry: New snapshot database entry to be exposed.
    @rtype: N/A
    @return: None
    """
    try:
        if os.path.exists(tmp_smb_conf1):
            os.system('sudo rm -rf %s' % tmp_smb_conf1)
        # wthung, 2012/8/1
        # add to remove tmp_smb_conf2
        if os.path.exists(tmp_smb_conf2):
            os.system('sudo rm -rf %s' % tmp_smb_conf2)
        snapshot_share_path = os.path.join(snapshot_dir, entry['name'])

        os.system('sudo touch %s' % tmp_smb_conf1)
        os.system('sudo chown www-data:www-data %s' % tmp_smb_conf1)

        with open(tmp_smb_conf1, 'w') as fh:
            fh.write('[%s]\n' % entry['name'])
            fh.write('comment = Samba Share for snapshot %s\n' % entry['name'])
            fh.write('path = %s\n' % snapshot_share_path)
            fh.write('browsable = yes\n')
            fh.write('guest ok = no\n')
            fh.write('read only = yes\n')
            fh.write('create mask = 0755\n')
            fh.write('valid users = superuser\n\n')
        
        os.system('sudo cat %s %s > %s' % (tmp_smb_conf, tmp_smb_conf1, tmp_smb_conf2))
        os.system('sudo cp %s %s' % (tmp_smb_conf2, tmp_smb_conf))
    except:
        raise SnapshotError('Unable to append entry to smb.conf')


def _write_snapshot_db(snapshot_list):
    """
    Write updated snapshot database entries to the database file.

    @type snapshot_list:  Array of snapshot database entries
    @param snapshot_list: Array of updated snapshot database entries
    @rtype:   N/A
    @return:  None
    """
    db_lock = Snapshot_Db_Lock()

    try:
        if os.path.exists(temp_snapshot_db):
            os.system('sudo rm -rf %s' % temp_snapshot_db)

        # Since the API is run from www-data account, we need to chown
        os.system('sudo touch %s' % temp_snapshot_db)
        os.system('sudo chown www-data:www-data %s' % temp_snapshot_db)

        with open(temp_snapshot_db, 'w') as fh:
            for entry in snapshot_list:
                if entry['exposed']:
                    is_exposed = 'true'
                else:
                    is_exposed = 'false'
                
                # wthung, 2012/7/17
                # add a entry of auto_exposed
                if entry['auto_exposed']:
                    is_auto_exposed = 'true'
                else:
                    is_auto_exposed = 'false'

                fh.write('%s,%f,%f,%d,%d,%s,%s\n' % (entry['name'],\
                        entry['start_time'], entry['finish_time'],\
                        entry['num_files'], entry['total_size'],\
                        is_exposed, is_auto_exposed))

        os.system('sudo cp %s %s' % (temp_snapshot_db, snapshot_db))

    except:
        raise SnapshotError('Unable to update snapshot database')
    finally:
        del db_lock


def expose_snapshot(to_expose, expose_after_creation=False):
    """
    API function for exposing snapshot entries as samba shares.

    @type to_expose: Array of strings
    @param to_expose: List of snapshot names to be exposed as samba shares.
    @rtype:    Json object
    @return:   A json object with function result and returned message.
    """
    log.info('Started get_snapshot_in_progress')
    return_result = False
    return_msg = '[2] Unexpected error in get_snapshot_in_progress'

    try:
        if not os.path.exists(org_smb_conf):
            os.system('sudo cp %s %s' % (smb_conf_file, org_smb_conf))

        if not os.path.exists(tmp_smb_dir):
            os.system('sudo mkdir %s' % tmp_smb_dir)
            os.system('sudo chown www-data:www-data %s' % tmp_smb_dir)

        db_list = _acquire_db_list()
        snapshot_list = _translate_db(db_list)

        # Initial tmp samba config
        os.system('sudo cp %s %s' % (org_smb_conf, tmp_smb_conf))

        # wthung, 2012/7/17
        # if auto_exposed was ever set to false, we should not set it to true anymore
        for entry in snapshot_list:
            if entry['name'] in to_expose:
                entry['exposed'] = True
                #_append_samba_entry(entry)
            else:
                # by expose_after_creation, we don't disable export on item which is not in input list
                # because in take_snapshot, we only input latest snapshot to export
                # this avoids other exported item to be disabled exporting
                if not expose_after_creation:
                    entry['exposed'] = False
                    entry['auto_exposed'] = False

        # we must iterate snapshots again for export 
        for entry in snapshot_list:
            if entry['exposed']:
                _append_samba_entry(entry)

        # Restart samba service
        os.system('sudo /etc/init.d/smbd stop')
        os.system('sudo /etc/init.d/nmbd stop')
        os.system('sudo cp %s %s' % (tmp_smb_conf, smb_conf_file))
        os.system('sudo /etc/init.d/smbd start')
        os.system('sudo /etc/init.d/nmbd start')

        log.info('Restarted samba service after snapshot exposing')

        # Write back snapshot database
        _write_snapshot_db(snapshot_list)

        return_result = True
        return_msg = 'Finished exposing snapshots as samba shares'

    except:
        return_msg = '[2] Unable to expose snapshot'

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'data': {}}
    return json.dumps(return_val)


def _search_index(snapshot_name, snapshot_list):
    """
    Helper function for finding the index of a snapshot name in the database.

    @type snapshot_name: String
    @param snapshot_name: Name of the snapshot that is being queried.
    @type snapshot_list: Array of snapshot database entries
    @param snapshot_list: Array of the current snapshot database entries
    @rtype:  Number
    @return: The index of the queried snapshot in the database (-1 if
             not found).
    """
    try:
        for index in range(len(snapshot_list)):
            if snapshot_list[index]['name'] == snapshot_name:
                return index
    except:
        raise SnapshotError('Unable to determine if the snapshot is exposed')

    return -1


def delete_snapshot(to_delete):
    """
    API function for deleting a snapshot entry.

    @type to_delete: String
    @param to_delete: Snapshot name to be deleted.
    @rtype:    Json object
    @return:   A json object with function result and returned message.
    """
    log.info('Started delete_snapshot')
    return_result = False
    return_msg = '[2] Unexpected error in delete_snapshot'

    try:
        db_list = _acquire_db_list()
        snapshot_list = _translate_db(db_list)

        snapshot_index = _search_index(to_delete, snapshot_list)

        if snapshot_index < 0:  # Cannot find the name of the snapshot
            return_msg = 'Unable to find the snapshot in the database'
        else:
            if not snapshot_list[snapshot_index]['exposed']:  # It is OK to delete
                snapshot_path = os.path.join(snapshot_dir, to_delete)
                if os.path.exists(snapshot_path):  # Invoke s3qlrm
                    os.system('sudo python /usr/local/bin/s3qlrm %s' % snapshot_path)

                updated_snapshot_list = snapshot_list[:snapshot_index] + snapshot_list[snapshot_index + 1:]
                _write_snapshot_db(updated_snapshot_list)

                return_result = True
                return_msg = 'Finished deleting snapshot'

            else:
                return_msg = 'Snapshot is currently being exposed. Skipping.'

    except:
        pass

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'data': {}}
    return json.dumps(return_val)


def _write_snapshot_lifespan(days_to_live):
    """
    Helper function for actually writing lifespan config to file

    @type days_to_live: Number
    @param days_to_live: The lifespan of a snapshot, measured in days.
    @rtype: N/A
    @return: None
    """
    try:
        if os.path.exists(lifespan_conf):
            os.system('rm -rf %s' % lifespan_conf)

        with open(lifespan_conf, 'w') as fh:
            fh.write('%d' % days_to_live)
        os.system('sudo chown www-data:www-data %s' % lifespan_conf)
    except:
        raise SnapshotError('Unable to write snapshot lifespan config to file')


def set_snapshot_lifespan(days_to_live):
    """
    API function for setting the lifespan of snapshots.

    @type days_to_live: Number
    @param days_to_live: The lifespan of a snapshot, measured in days.
    @rtype:    Json object
    @return:   A json object with function result and returned message.
    """
    log.info('Started set_snapshot_lifespan')
    return_result = False
    return_msg = '[2] Unexpected error in get_snapshot_lifespan'

    try:
        if days_to_live >= 0:
            _write_snapshot_lifespan(days_to_live)
            return_result = True
            return_msg = 'Completed set_snapshot_lifespan'
        else:
            return_msg = 'Error in set_snapshot_lifespan: days_to_live must be a positive integer or zero.'
    except Exception as Err:
        return_msg = str(Err)

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'data': {}}
    return json.dumps(return_val)


def get_snapshot_lifespan():
    """
    API function for getting the lifespan of snapshots.

    If the lifespan config does not exists, the default value (365 days)
    is returned and also written to the config file.

    @rtype:    Json object
    @return:   A json object with function result ,returned message, and
               the current lifespan (in days) of the snapshots.
    """
    log.info('Started get_snapshot_lifespan')
    days_to_live = 365
    return_result = False
    return_msg = '[2] Unexpected error in get_snapshot_lifespan'
    reset_config = False

    try:
        with open(lifespan_conf, 'r') as fh:
            data_in = fh.readline()
            try:
                days_to_live = int(data_in)
                return_result = True
                return_msg = 'Completed get_snapshot_lifespan'
            except:
                log.info('Stored lifespan is not an integer. Resetting to default (365 days).')
                reset_config = True
    except IOError:
        log.info('Unable to open config for snapshot lifespan. Resetting to default (365 days).')
        reset_config = True
    except:
        pass

    if reset_config:  # If we need to use the default value as returned value
        try:
            _write_snapshot_lifespan(365)
            days_to_live = 365
            return_result = True
            return_msg = 'Completed get_snapshot_lifespan'
        except SnapshotError as Err:
            return_msg = str(Err)

    log.info(return_msg)
    return_val = {'result': return_result,
                  'msg': return_msg,
                  'data': {'days_to_live': days_to_live}}
    return json.dumps(return_val)

# wthung, 2012/8/3
def _parse_snapshot_time(ss_dirname):
    """
    Parse snapshot time from snapshot directory name.
    
    @type ss_dirname: String
    @param ss_dirname: Snapshot directory name
    @rtype: Integer
    @return: Snapshot time since 1970
    """
    
    # sample snapshot dirname: snapshot_year_month_day_hour_minute_second
    ret = -1
    try:
        #(prefix, year, month, day, hour, minute, second) = ss_dirname.split('_')
        prefix, year, month, day, hour, minute, second = ss_dirname.split('_')
        if prefix == 'snapshot':
            cur_struct_time = time.strptime("%s %s %s %s %s %s" % (year, 
                                                                   month,
                                                                   day,
                                                                   hour,
                                                                   minute,
                                                                   second),
                                            "%Y %m %d %H %M %S")
            sec_since_epoch = time.mktime(cur_struct_time)
            ret = int(sec_since_epoch)
    except Exception as e:
        log.info('[0] Error occurred when parsing snapshot directory name: %s' % str(e))
    
    return ret


def _get_snapshot_file_info(ss_dirname):
    """
    Get file number and total size of a snapshot directory.
    
    @type ss_dirname: String
    @param ss_dirname: Snapshot directory name
    @rtype: List
    @return: A list which file number is the first and total size is the second element,
             where total size is in bytes
    """
    
    ret = []
    
    # get file number, including sub-directories
    cmd = "sudo du -a %s | wc -l" % ss_dirname
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()
    
    file_num = int(output) - 3
    # error handling
    if (file_num < 0):
        file_num = 0
    
    # get total size, including sub-directories
    cmd = "sudo du -s %s" % ss_dirname
    po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()
    
    total_size, _ = output.split('\t')
    total_size = int(total_size)
    # for empty folder, total size calculated by "du -s" is 12
    # error handling
    if (total_size < 0):
        total_size = 0
    
    # append result
    ret.append(file_num)
    ret.append(total_size)
    
    return ret


def rebuild_snapshot_database(_ss_dir=None):
    """
    Rebuild snapshot database.
    
    @type _ss_dir: String
    @param _ss_dir: Base snapshot directory. If None is specified, use system default one
    """
    
    listing = []
    db_string = ''
    ss_folder = ''
    
    if _ss_dir is None:
        ss_folder = snapshot_dir
    else:
        ss_folder = _ss_dir

    # ensure snapshots folder is existed
    if not os.path.exists(ss_folder):
        log.info('[2] Skip rebuilding snapshot DB due to no snapshot was found')
        return
        
    listing = os.listdir(ss_folder)
    
    db_lock = Snapshot_Db_Lock()
    
    try:
        # process most recent snapshot
        for dirname in reversed(listing):
            ss_time = _parse_snapshot_time(dirname)
            if ss_time != -1:
                file_num, total_size = _get_snapshot_file_info("%s/%s" % (ss_folder, dirname))
                db_string += "%s,%d,%d,%d,%d,false,false\n" % (dirname, ss_time, ss_time, file_num, total_size >> 10)
        
        # remove the last newline
        #db_string = db_string[:-1]
        
        # backup snapshot db if any
        if os.path.exists(snapshot_db):
            os.system('sudo mv %s %s.bak' % (snapshot_db, snapshot_db))
        
        with open(snapshot_db, 'w') as fh:
            fh.write(db_string)
        
    except Exception as e:
        log.info('[0] Failed to rebuild snapshot database: %s' % str(e))
        print('[0] Failed to rebuild snapshot database: %s' % str(e))
    finally:
        del db_lock


################################################################

if __name__ == '__main__':
    #print delete_snapshot('snapshot_2012_6_25_17_46_24')
    rebuild_snapshot_database()
