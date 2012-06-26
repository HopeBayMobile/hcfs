"""
This script is part of Delta Cloud Storage Gateway API functions
Developed by CTBU, Delta Electronics Inc., 2012

This is the main bot script for taking snapshots.
"""
import os.path
import common
import subprocess
import time
from datetime import datetime

log = common.getLogger(name="API", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

snapshot_tag = "/root/.s3ql/.snapshotting"
snapshot_db = "/root/.s3ql/snapshot_db.txt"
temp_snapshot_db = "/root/.s3ql/.tempsnapshotdb"
temp_snapshot_db1 = "/root/.s3ql/.tempsnapshotdb1"
temp_folder = "/mnt/cloudgwfiles/tempsnapshot"
snapshot_statistics = "/root/.s3ql/snapshot.log"
samba_folder = "/mnt/cloudgwfiles/sambashare"
nfs_folder = "/mnt/cloudgwfiles/nfsshare"
mount_point = "/mnt/cloudgwfiles"
snapshot_dir = "/mnt/cloudgwfiles/snapshots"
samba_user = "superuser:superuser"
snapshot_db_lock = "/root/.s3ql/.snapshot_db_lock"


class SnapshotError(Exception):
    pass


class Snapshot_Db_Lock():
    """
    Same snapshot database lock class as in the API.
    """
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
    """
    Helper function for checking if a snapshot process exists.

    @rtype: Boolean value
    @Return: Whether a snapshotting process is in progress.
    """
    try:
        if os.path.exists(snapshot_tag):
            return True
        return False
    except:
        raise\
          SnapshotError("Could not decide whether a snapshot is in progress.")


def new_database_entry():
    """
    Create a new entry in the database open the start of the snapshotting.

    Function will retry up to 10 times.
    """

    retries = 10
    finished = False

    while not finished:
        db_lock = Snapshot_Db_Lock()
        try:
            if os.path.exists(temp_snapshot_db):
                os.system('rm -rf %s' % temp_snapshot_db)

            with open(temp_snapshot_db, 'w') as fh:
                fh.write('new_snapshot,%f,-1,0,0,false\n' % time.time())

            if not os.path.exists(snapshot_db):
                os.system('sudo mv %s %s' % (temp_snapshot_db, snapshot_db))
            else:
                os.system('sudo cat %s %s > %s' % (temp_snapshot_db,\
                               snapshot_db, temp_snapshot_db1))
                os.system('sudo cp %s %s' % (temp_snapshot_db1, snapshot_db))

            finished = True

        except:
            retries = retries - 1
            time.sleep(5)
            if retries <= 0:
                error_msg = "Could not write new entry to snapshot database"
                raise SnapshotError(error_msg)
        finally:
            del db_lock


def invalidate_entry():
    """
    Invalidate snapshot database entry and label it as failed snapshot.
    """

    finish_time = time.time()
    new_snapshot_name = "Failed_snapshot"
    update_new_entry(new_snapshot_name, finish_time, 0, 0)


def actually_not_in_progress():
    """
    Check if there is no 'new_snapshot' entry in database.

    Reasoning behind this function: Snapshotting might already completed,
    but the post-processing is not and a system failure occurs. Hence it
    is possible that the snapshot tag exists but the database is already
    updated.
    """

    db_lock = Snapshot_Db_Lock()
    try:
        with open(snapshot_db, 'r') as fh:
            db_lines = fh.readlines()
            if len(db_lines) < 1:
                no_new_entry = True
            else:
                first_entries = db_lines[0].split(',')
                if first_entries[0] != 'new_snapshot':
                    no_new_entry = True
                else:
                    no_new_entry = False
    except:
        raise SnapshotError('Unable to determine if snapshot is in progress')
    finally:
        del db_lock

    return no_new_entry


def recover_database():
    """
    Fix the snapshot entry and database.

    Note: The database could be updated but the snapshot directory is not.

    Tag is removed at the end of the function.
    """
    retries = 10
    finished = False

    while not finished:

        db_lock = Snapshot_Db_Lock()
        try:
            #First check if the temp snapshot folder still exists
            if os.path.exists(temp_folder):
                #Check if the path of the newest snapshot exists
                with open(snapshot_db, 'r') as fh:
                    first_line = fh.readline()
                    first_entries = first_line.split(',')
                    new_snapshot_name = first_entries[0]
                    new_snapshot_path = os.path.join(snapshot_dir, new_snapshot_name)

                if not os.path.exists(new_snapshot_path):
                    # This should be the missing snapshot folder. Rename the temp folder.
                    if not os.path.exists(snapshot_dir):
                        os.system('sudo mkdir %s' % snapshot_dir)
                    os.system('sudo mv %s %s' % (temp_folder, new_snapshot_path))
                    os.system('sudo chown %s %s' % (samba_user, new_snapshot_path))
                    os.system('sudo python /usr/local/bin/s3qllock %s' % (new_snapshot_path))
                    log.info('Recovered snapshot' % new_snapshot_name)
                else:
                    # Perhaps the first entry is missing. Create another snapshot entry.
                    finish_time = time.time()
                    ftime_format = time.localtime(finish_time)
                    new_snapshot_name = "recovered_snapshot_%s_%s_%s_%s_%s_%s" % (ftime_format.tm_year,\
                             ftime_format.tm_mon, ftime_format.tm_mday, ftime_format.tm_hour,\
                             ftime_format.tm_min, ftime_format.tm_sec)
                    update_new_entry(new_snapshot_name, finish_time, 0, 0)

                    new_snapshot_path = os.path.join(snapshot_dir, new_snapshot_name)

                    os.system('sudo mv %s %s' % (temp_folder, new_snapshot_path))
                    os.system('sudo chown %s %s' % (samba_user, new_snapshot_path))
                    os.system('sudo python /usr/local/bin/s3qllock %s' % (new_snapshot_path))
                    log.info('Recovered snapshot' % new_snapshot_name)
            else:
                try:  # We just need to lock the snapshot dir and change the ownership
                    with open(snapshot_db, 'r') as fh:
                        first_line = fh.readline()
                        first_entries = first_line.split(',')
                        new_snapshot_name = first_entries[0]
                        new_snapshot_path = os.path.join(snapshot_dir, new_snapshot_name)

                    os.system('sudo chown %s %s' % (samba_user, new_snapshot_path))
                    os.system('sudo python /usr/local/bin/s3qllock %s' % (new_snapshot_path))
                except:
                    pass  # Perhaps we already had done these

            os.system('sudo python /usr/local/bin/s3qlctrl upload-meta %s' % mount_point)
            os.system('sudo rm -rf %s' % snapshot_tag)
            finished = True

        except:
            retries = retries - 1
            time.sleep(5)
            if retries <= 0:
                error_msg = "Could not recover snapshot database"
                raise SnapshotError(error_msg)
        finally:
            del db_lock


def update_new_entry(new_snapshot_name, finish_time, total_files, total_size):
    """
    Update the snapshot database entry upon the completion of the snapshot.

    @type new_snapshot_name: String
    @param new_snapshot_name: Name of the snapshot entry
    @type finish_time: Number
    @param finish_time: Finish time of the snapshot process (secs after epoch)
    @type total_files: Integer
    @param total_files: Total number of files in the snapshot
    @type total_size: Number
    @param total_size: Total amount of data in the snapshot
    """
    retries = 10
    finished = False

    while not finished:

        db_lock = Snapshot_Db_Lock()
        try:
            with open(snapshot_db, 'r') as fh:
                db_lines = fh.readlines()
                if len(db_lines) < 1:
                    create_new_entry = True
                else:
                    first_entries = db_lines[0].split(',')
                    if first_entries[0] != 'new_snapshot':
                        create_new_entry = True
                    else:
                        create_new_entry = False
                if create_new_entry:
                    log.info('Missing new snapshot entry. Adding a new entry.')
                    start_time = finish_time
                else:
                    start_time = float(first_entries[1])

            if os.path.exists(temp_snapshot_db):
                os.system('sudo rm -rf %s' % temp_snapshot_db)

            with open(temp_snapshot_db, 'w') as fh:
                fh.write('%s,%f,%f,%d,%d,false\n' % (new_snapshot_name,\
                       start_time, finish_time, total_files, total_size))
                if create_new_entry:
                    #Write the original db
                    fh.writelines(db_lines)
                else:
                    #Skip the first line of the original db
                    fh.writelines(db_lines[1:])

            os.system('sudo cp %s %s' % (temp_snapshot_db, snapshot_db))
            finished = True

        except:
            retries = retries - 1
            time.sleep(5)
            if retries <= 0:
                error_msg = "Could not update new entry in snapshot database"
                raise SnapshotError(error_msg)
        finally:
            del db_lock


def execute_take_snapshot():
    """
    Main function for taking the snapshots.
    """
    log.info('Begin snapshotting bot tasks')

    if not _check_snapshot_in_progress():
        # if we are initializing a new snapshot process
        try:
            os.system("sudo touch %s" % snapshot_tag)  # Tag snapshotting
            new_database_entry()
        except SnapshotError as Err:
            err_msg = str(Err)
            log.info('[2] Unexpected error in snapshotting.')
            log.info('Error message: %s' % err_msg)
            return
    else:
        if actually_not_in_progress():
            # Check if there is actually a "new_snapshot" entry in database
            recover_database()    # Fix the snapshot entry and database
# Note: The database could be updated but the snapshot directory is not
# renamed or locked. Remove tag after this.
            return
        else:  # A snapshotting is in progress
            pass

    finish = False

    while not finish:
        try:
            # Check if file system is still up
            if not os.path.exists(samba_folder):
                return

            if os.path.exists(temp_folder):
                os.system("sudo rm -rf %s" % temp_folder)

            if os.path.exists(snapshot_statistics):
                os.system('sudo rm -rf %s' % snapshot_statistics)

            # Attempt the actual snapshotting again

            os.system("sudo mkdir %s" % temp_folder)
            cmd = "sudo python /usr/local/bin/s3qlcp %s %s/sambashare"\
                                % (samba_folder, temp_folder)
            po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,\
                                stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()

            if po.returncode != 0:
                if output.find("Dirty cache has not been completely flushed")\
                              != -1:
                    time.sleep(60)  # Wait one minute before retrying
                else:
                    # Check if file system is still up
                    if not os.path.exists(samba_folder):
                        return

                    invalidate_entry()  # Cannot finish the snapshot for some reason
                    log.info('[2] Unable to finish the current snapshotting process. Aborting.')
                    os.system('sudo rm -rf %s' % snapshot_tag)
                    return
            else:
                # Record statistics for samba share
                with open(snapshot_statistics, 'r') as fh:
                    for lines in fh:
                        if lines.find("total files") != -1:
                            samba_files = int(lines.lstrip('toal fies:'))
                        if lines.find("total size") != -1:
                            samba_size = int(lines.lstrip('toal size:'))
                samba_size = int(samba_size / 1000000)
                os.system('sudo rm -rf %s' % snapshot_statistics)

                cmd = "sudo python /usr/local/bin/s3qlcp %s %s/nfsshare" % (nfs_folder, temp_folder)
                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                output = po.stdout.read()
                po.wait()

                if po.returncode != 0:
                    if output.find("Dirty cache has not been completely flushed") != -1:
                        time.sleep(60)  # Wait one minute before retrying
                    else:
                        # Check if file system is still up
                        if not os.path.exists(samba_folder):
                            return

                        invalidate_entry()
                        log.info('[2] Unable to finish the current snapshotting process. Aborting.')
                        os.system('sudo rm -rf %s' % snapshot_tag)
                        return
                else:
                    # Record statistics for NFS share
                    with open(snapshot_statistics, 'r') as fh:
                        for lines in fh:
                            if lines.find("total files") != -1:
                                nfs_files = int(lines.lstrip('toal fies:'))
                            if lines.find("total size") != -1:
                                nfs_size = int(lines.lstrip('toal size:'))
                    nfs_size = int(nfs_size / 1000000)
                    os.system('sudo rm -rf %s' % snapshot_statistics)

                    finish = True
        except Exception:
            # Check if file system is still up
            if not os.path.exists(samba_folder):
                return
            raise

    # Begin post-processing snapshotting
    # Update database
    finish_time = time.time()
    ftime_format = time.localtime(finish_time)
    new_snapshot_name = "snapshot_%s_%s_%s_%s_%s_%s" % (ftime_format.tm_year,\
              ftime_format.tm_mon, ftime_format.tm_mday, ftime_format.tm_hour,\
              ftime_format.tm_min, ftime_format.tm_sec)
    update_new_entry(new_snapshot_name, finish_time, samba_files + nfs_files,\
              samba_size + nfs_size)


    # Make necessary changes to the directory structure and lock the snapshot
    if not os.path.exists(snapshot_dir):
        os.system('sudo mkdir %s' % snapshot_dir)
    new_snapshot_path = os.path.join(snapshot_dir, new_snapshot_name)
    os.system('sudo mv %s %s' % (temp_folder, new_snapshot_path))
    os.system('sudo chown %s %s' % (samba_user, new_snapshot_path))
    os.system('sudo python /usr/local/bin/s3qllock %s' % (new_snapshot_path))
    os.system('sudo python /usr/local/bin/s3qlctrl upload-meta %s' % mount_point)
    os.system('sudo rm -rf %s' % snapshot_tag)

    log.info('Snapshotting finished at %s' % str(ftime_format))

################################################################

if __name__ == '__main__':
    try:
        execute_take_snapshot()
    except Exception as err:
        log.info('%s' % str(err))
