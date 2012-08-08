"""
This script is part of Delta Cloud Storage Gateway API functions
Developed by CTBU, Delta Electronics Inc., 2012

This script checks for and deletes expired snapshots.
"""
import os.path
import common
import time
from datetime import datetime

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

snapshot_tag = "/root/.s3ql/.snapshotting"
snapshot_bot = "/etc/delta/snapshot_bot"
snapshot_db_lock = "/root/.s3ql/.snapshot_db_lock"
snapshot_db = "/root/.s3ql/snapshot_db.txt"
snapshot_dir = "/mnt/cloudgwfiles/snapshots"
lifespan_conf = "/etc/delta/snapshot_lifespan"
temp_snapshot_db = "/root/.s3ql/.tempsnapshotdb"


####### Exception class definition ###############


class SnapshotError(Exception):
    pass


####### Start of functions ####################


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

    @type db_list:  Array of strings
    @param db_list: Lines in the snapshot database (as array of strings).
    @rtype:         Array of python dictionaries
    @return:        Entries in the snapshot database.
    """
    snapshots = []
    try:
        for entry in db_list:
            tmp_items = entry.split(',')

            if tmp_items[5] == 'true\n':
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
    except:
        raise SnapshotError('Unable to convert snapshot db')
    return snapshots


def _write_snapshot_db(snapshot_list):
    """
    Write updated snapshot database entries to the database file.
    Note that this is not the same version as the one in the API.

    @type snapshot_list:  Array of snapshot database entries
    @param snapshot_list: Array of updated snapshot database entries
    @rtype:   N/A
    @return:  None
    """
    db_lock = Snapshot_Db_Lock()

    try:
        if os.path.exists(temp_snapshot_db):
            os.system('sudo rm -rf %s' % temp_snapshot_db)

        # We don't need to chown to www-data here as the script is
        # run from a cron job

        with open(temp_snapshot_db, 'w') as fh:
            for entry in snapshot_list:
                if entry['exposed']:
                    is_exposed = 'true'
                else:
                    is_exposed = 'false'

                fh.write('%s,%f,%f,%d,%d,%s\n' % (entry['name'],\
                       entry['start_time'], entry['finish_time'],\
                       entry['num_files'], entry['total_size'],\
                       is_exposed))

        os.system('sudo cp %s %s' % (temp_snapshot_db, snapshot_db))

    except:
        raise SnapshotError('Unable to update snapshot database')
    finally:
        del db_lock


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


def _get_snapshot_lifespan():
    """
    Helper function for getting the lifespan of snapshots.

    @rtype:    Number
    @return:   Lifespan of snapshots (in days).
    """
    days_to_live = 365
    reset_config = False

    try:
        with open(lifespan_conf, 'r') as fh:
            data_in = fh.readline()
            try:
                days_to_live = int(data_in)
            except:
                log.info('Stored lifespan is not an integer. Resetting to default (365 days).')
                reset_config = True
    except IOError:
        log.info('Unable to open config for snapshot lifespan. Resetting to default (365 days).')
        reset_config = True
    except:
        pass

    if reset_config:
        _write_snapshot_lifespan(365)
        days_to_live = 365

    return days_to_live


def check_expired_snapshots():
    """
    The main function for checking for and deleting expired
    snapshots.

    @rtype: N/A
    @return: None
    """
    log.info('Start checking expired snapshots')

    try:
        finished = False
        snapshot_lifetime = _get_snapshot_lifespan()
        current_time = time.time()

        while not finished:
            db_list = _acquire_db_list()
            snapshot_list = _translate_db(db_list)
            finished = True

            for index in reversed(range(len(snapshot_list))):
                if snapshot_list[index]['exposed']:
                    pass
                else:
                    # Check if this snapshot is expired
                    time_diff = current_time - snapshot_list[index]['finish_time']
                    if time_diff > snapshot_lifetime * 86400:
                        # Snapshot expired
                        to_delete = snapshot_list[index]['name']
                        finished = False
                        snapshot_index = index
                        break

            if finished:
                break

            # Delete expired snapshots
            log.info('Deleting snapshot %s' % to_delete)
            snapshot_path = os.path.join(snapshot_dir, to_delete)
            if os.path.exists(snapshot_path):  # Invoke s3qlrm
                os.system('sudo python /usr/local/bin/s3qlrm %s' % snapshot_path)

            updated_snapshot_list = snapshot_list[:snapshot_index] + snapshot_list[snapshot_index + 1:]
            _write_snapshot_db(updated_snapshot_list)

        log.info('Finished checking expired snapshots')

    except Exception as Err:
        raise SnapshotError(str(Err))


################################################################

if __name__ == '__main__':
    try:
        check_expired_snapshots()
    except Exception as err:
        log.info('Error in checking expired snapshots')
        log.info('%s' % str(err))
