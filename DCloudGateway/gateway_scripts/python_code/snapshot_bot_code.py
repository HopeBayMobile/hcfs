"""
This script is part of Delta Cloud Storage Gateway API functions
Developed by CTBU, Delta Electronics Inc., 2012

This is the main bot script for taking snapshots.
"""
import os.path
import subprocess
import time
import urllib2
import json
from datetime import datetime
from gateway import snapshot
from gateway import common

log = common.getLogger(name="Snapshot_Bot", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

temp_folder = "/mnt/cloudgwfiles/tempsnapshot"
snapshot_statistics = "/root/.s3ql/snapshot.log"
samba_folder = "/mnt/cloudgwfiles/sambashare"
nfs_folder = "/mnt/cloudgwfiles/nfsshare"
mount_point = "/mnt/cloudgwfiles"
samba_user = "superuser:superuser"

def new_database_entry():
    """
    Create a new entry in the database open the start of the snapshotting.

    Function will retry up to 10 times.
    """

    retries = 10
    finished = False

    while not finished:
        db_lock = snapshot.Snapshot_Db_Lock()
        try:
            if os.path.exists(snapshot.temp_snapshot_db):
                os.system('rm -rf %s' % snapshot.temp_snapshot_db)

            with open(snapshot.temp_snapshot_db, 'w') as fh:
                # wthung, 2012/7/17
                # add last field to represent auto_exposed. default is true
                fh.write('new_snapshot,%f,-1,0,0,false,true\n' % time.time())

            if not os.path.exists(snapshot.snapshot_db):
                os.system('sudo mv %s %s' % (snapshot.temp_snapshot_db, snapshot.snapshot_db))
            else:
                os.system('sudo cat %s %s > %s' % (snapshot.temp_snapshot_db,\
                               snapshot.snapshot_db, snapshot.temp_snapshot_db1))
                os.system('sudo cp %s %s' % (snapshot.temp_snapshot_db1, snapshot.snapshot_db))

            finished = True

        except:
            retries = retries - 1
            time.sleep(5)
            if retries <= 0:
                error_msg = "Could not write new entry to snapshot database"
                raise snapshot.SnapshotError(error_msg)
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

    db_lock = snapshot.Snapshot_Db_Lock()
    try:
        with open(snapshot.snapshot_db, 'r') as fh:
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
        raise snapshot.SnapshotError('Unable to determine if snapshot is in progress')
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

        db_lock = snapshot.Snapshot_Db_Lock()
        try:
            #First check if the temp snapshot folder still exists
            if os.path.exists(temp_folder):
                #Check if the path of the newest snapshot exists
                with open(snapshot.snapshot_db, 'r') as fh:
                    first_line = fh.readline()
                    first_entries = first_line.split(',')
                    new_snapshot_name = first_entries[0]
                    new_snapshot_path = os.path.join(snapshot.snapshot_dir, new_snapshot_name)

                if not os.path.exists(new_snapshot_path):
                    # This should be the missing snapshot folder. Rename the temp folder.
                    if not os.path.exists(snapshot.snapshot_dir):
                        os.system('sudo mkdir %s' % snapshot.snapshot_dir)
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

                    new_snapshot_path = os.path.join(snapshot.snapshot_dir, new_snapshot_name)

                    os.system('sudo mv %s %s' % (temp_folder, new_snapshot_path))
                    os.system('sudo chown %s %s' % (samba_user, new_snapshot_path))
                    os.system('sudo python /usr/local/bin/s3qllock %s' % (new_snapshot_path))
                    log.info('Recovered snapshot' % new_snapshot_name)
            else:
                try:  # We just need to lock the snapshot dir and change the ownership
                    with open(snapshot.snapshot_db, 'r') as fh:
                        first_line = fh.readline()
                        first_entries = first_line.split(',')
                        new_snapshot_name = first_entries[0]
                        new_snapshot_path = os.path.join(snapshot.snapshot_dir, new_snapshot_name)

                    os.system('sudo chown %s %s' % (samba_user, new_snapshot_path))
                    os.system('sudo python /usr/local/bin/s3qllock %s' % (new_snapshot_path))
                except:
                    pass  # Perhaps we already had done these

            os.system('sudo python /usr/local/bin/s3qlctrl upload-meta %s' % mount_point)
            os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
            finished = True

        except:
            retries = retries - 1
            time.sleep(5)
            if retries <= 0:
                error_msg = "Could not recover snapshot database"
                raise snapshot.SnapshotError(error_msg)
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

        db_lock = snapshot.Snapshot_Db_Lock()
        try:
            with open(snapshot.snapshot_db, 'r') as fh:
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

            if os.path.exists(snapshot.temp_snapshot_db):
                os.system('sudo rm -rf %s' % snapshot.temp_snapshot_db)

            with open(snapshot.temp_snapshot_db, 'w') as fh:
                # wthung, 2012/7/17
                # add last field to represent auto_exposed. default to true
                fh.write('%s,%f,%f,%d,%d,false,true\n' % (new_snapshot_name,\
                       start_time, finish_time, total_files, total_size))
                if create_new_entry:
                    #Write the original db
                    fh.writelines(db_lines)
                else:
                    #Skip the first line of the original db
                    fh.writelines(db_lines[1:])

            os.system('sudo cp %s %s' % (snapshot.temp_snapshot_db, snapshot.snapshot_db))
            finished = True

        except:
            retries = retries - 1
            time.sleep(5)
            if retries <= 0:
                error_msg = "Could not update new entry in snapshot database"
                raise snapshot.SnapshotError(error_msg)
        finally:
            del db_lock

def _get_snapshot_task():
    get_url = "http://127.0.0.1:80/restful/services/snapshot2/task/get" 
    req = urllib2.Request(get_url)
    code = -1
    response = None
    
    try:
        f = urllib2.urlopen(req)
        code = f.getcode()
        response = f.read()
        f.close()
    except urllib2.HTTPError as e:
        code = e.code
        response = e.read()
    except urllib2.URLError as e:
        response = e.reason
    except Exception as e:
        response = str(e)

    return {"code": code, "response": response}

def _post_snapshot_task(sid, sf_uid, src_folder, dest_folder, start_time, finish_time, file_no, size, result, err_msg):
    values = {"_id": sid,
              "sf_uid": sf_uid,
              "source": src_folder,
              "destination": dest_folder,
              "start_time": int(start_time),
              "stop_time": int(finish_time),
              "file_no": file_no,
              "size": size,
              "result": result,
              "err_msg": err_msg}
    #print('set snapshot task')
    #print values
    data = json.dumps(values)
    post_url = "http://127.0.0.1:80/restful/services/snapshot2/result/set"
    req = urllib2.Request(post_url, data, {'Content-Type': 'application/json'})
    code = -1
    response = None

    try:
        f = urllib2.urlopen(req)
        code = f.getcode()
        response = f.read()
        f.close()
    except urllib2.HTTPError as e:
        code = e.code
        response = e.read()
    except urllib2.URLError as e:
        response = e.reason
    except Exception as e:
        response = str(e)
    #print('after set snapshot task')
    #print('code: %d, response=%s' % (code, response))
    return {"code": code, "response": response}
        
def execute_take_snapshot():
    """
    Main function for taking the snapshots.
    """
    log.debug('Begin snapshotting bot tasks')

    while True:
        try:
            # check if ss statistics file exists, if yes, delete it
            if os.path.exists(snapshot_statistics):
                os.system('sudo rm -rf %s' % snapshot_statistics)
                
            # mark tag
            os.system("sudo touch %s" % snapshot.snapshot_tag)  # Tag snapshotting
            
            # get snapshot info from rest api
            ret_val = _get_snapshot_task()
            response = json.loads(ret_val["response"])
            #print('get task:')
            #print response
            
            if ret_val["code"] != 200:
                log.error('Failed to get snapshot task. Response: %s' % response)
                os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
                break

            # here we think if statuscode != 200, it means no snapshot task
            if response["statuscode"] != 200:
                os.system('sudo python /usr/local/bin/s3qlctrl upload-meta %s' % mount_point)
                os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
                log.debug('Snapshot: no snapshot task anymore')
                break

            # get id, src folder, dest folder
            sid = response["_id"]
            src_folder = response["source"]
            dest_folder = response["destination"]
            sf_uid = response["sf_uid"]
            #print('id=%s, src=%s, dest=%s' % (sid, src_folder, dest_folder))
            #src_folder = '/mnt/cloudgwfiles/sambashare/test111'
            #dest_folder = '/mnt/cloudgwfiles/sambashare/test222'
            
            # check if src folder does exist
            if not os.path.exists(src_folder):
                err_msg = "Snapshot: source folder doesn't exist"
                ret_val = _post_snapshot_task(sid, sf_uid, src_folder, dest_folder, 0, 0, 0, 0, False, err_msg)
                response = json.loads(ret_val["response"])
                if ret_val["code"] != 200 or response["statuscode"] != 200:
                    log.error('Failed to report snapshot task status (id: %s)' % (sid))
                time.sleep(3)
                continue
            
            # check if dest folder does not exist
            if os.path.exists(dest_folder):
                err_msg = "Snapshot: destination folder does exist"
                ret_val = _post_snapshot_task(sid, sf_uid, src_folder, dest_folder, 0, 0, 0, 0, False, err_msg)
                response = json.loads(ret_val["response"])
                if ret_val["code"] != 200 or response["statuscode"] != 200:
                    log.error('Failed to report snapshot task status (id: %s)' % (sid))
                time.sleep(3)
                continue
                    
            start_time = time.time()

            cmd = "sudo python /usr/local/bin/s3qlcp %s %s" % (src_folder, dest_folder)
            po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            output = po.stdout.read()
            po.wait()

            if po.returncode != 0:
                if output.find("Dirty cache has not been completely flushed") != -1:
                    # folder will be created in advance, so we need to delete it
                    if os.path.exists(dest_folder):
                        os.system('sudo rm -rf %s' % dest_folder)
                    time.sleep(60)  # Wait one minute before retrying
                else:
                    # s3qlcp failed
                    err_msg = 'Unable to finish the current snapshotting process. Aborting.'
                    log.error(err_msg)
                    os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
                    
                    finish_time = time.time()
                    # call rest api to report status
                    ret_val = _post_snapshot_task(sid, sf_uid, src_folder, dest_folder, start_time, finish_time, 0, 0, False, err_msg)
                    response = json.loads(ret_val["response"])
                    if ret_val["code"] != 200 or response["statuscode"] != 200:
                        log.error('Failed to report snapshot task status (id: %s)' % (sid))
                    time.sleep(3)
                    continue
            else:
                # Record statistics for samba share
                if not os.path.exists(snapshot_statistics):
                    # Statistics does not exists
                    err_msg = 'Unable to read snapshot statistics. Do we have the latest S3QL?'
                    log.warning(err_msg)
                    os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
                    
                    finish_time = time.time()
                    # call rest api to report status
                    ret_val = _post_snapshot_task(sid, sf_uid, src_folder, dest_folder, start_time, finish_time, 0, 0, False, err_msg)
                    response = json.loads(ret_val["response"])
                    if ret_val["code"] != 200 or response["statuscode"] != 200:
                        log.error('Failed to report snapshot task status (id: %s). Response: %s' % (sid, response))
                    time.sleep(3)
                    continue

                # size unit: bytes                
                with open(snapshot_statistics, 'r') as fh:
                    for lines in fh:
                        if lines.find("total files") != -1:
                            ss_files = int(lines.lstrip('total files:'))
                        if lines.find("total size") != -1:
                            ss_size = int(lines.lstrip('total size:'))
                #print('size=%d, files=%d' % (ss_size, ss_files))
                #ss_size = int(ss_size / 1000000)
                os.system('sudo rm -rf %s' % snapshot_statistics)
                finish_time = time.time()
                
                os.system('sudo python /usr/local/bin/s3qllock %s' % (dest_folder))
                os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
                
                ftime_format = time.localtime(finish_time)
                log.debug('Snapshotting finished at %s. (src: %s, dest: %s)' % (str(ftime_format), src_folder, dest_folder))
                
                # call rest api to report status
                ret_val = _post_snapshot_task(sid, sf_uid, src_folder, dest_folder, start_time, finish_time, ss_files, ss_size, True, None)
                response = json.loads(ret_val["response"])
                if ret_val["code"] != 200 or response["statuscode"] != 200:
                    log.error('Failed to report snapshot task status (id: %s)' % (sid))

                time.sleep(3)
        except Exception:
            raise

#def execute_take_snapshot():
#    """
#    Main function for taking the snapshots.
#    """
#    log.debug('Begin snapshotting bot tasks')
#
#    if not snapshot._check_snapshot_in_progress():
#        # if we are initializing a new snapshot process
#        try:
#            os.system("sudo touch %s" % snapshot.snapshot_tag)  # Tag snapshotting
#            new_database_entry()
#        except snapshot.SnapshotError as Err:
#            err_msg = str(Err)
#            log.error('Unexpected error in snapshotting.')
#            log.error('Error message: %s' % err_msg)
#            return
#    else:
#        if actually_not_in_progress():
#            # Check if there is actually a "new_snapshot" entry in database
#            recover_database()    # Fix the snapshot entry and database
## Note: The database could be updated but the snapshot directory is not
## renamed or locked. Remove tag after this.
#            return
#        else:  # A snapshotting is in progress
#            pass
#
#    finish = False
#
#    while not finish:
#        try:
#            # Check if file system is still up
#            if not os.path.exists(samba_folder):
#                return
#
#            if os.path.exists(temp_folder):
#                os.system("sudo rm -rf %s" % temp_folder)
#
#            if os.path.exists(snapshot_statistics):
#                os.system('sudo rm -rf %s' % snapshot_statistics)
#
#            # Attempt the actual snapshotting again
#
#            os.system("sudo mkdir %s" % temp_folder)
#            cmd = "sudo python /usr/local/bin/s3qlcp %s %s/sambashare"\
#                                % (samba_folder, temp_folder)
#            po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,\
#                                stderr=subprocess.STDOUT)
#            output = po.stdout.read()
#            po.wait()
#
#            if po.returncode != 0:
#                if output.find("Dirty cache has not been completely flushed")\
#                              != -1:
#                    time.sleep(60)  # Wait one minute before retrying
#                else:
#                    # Check if file system is still up
#                    if not os.path.exists(samba_folder):
#                        return
#
#                    invalidate_entry()  # Cannot finish the snapshot for some reason
#                    log.error('Unable to finish the current snapshotting process. Aborting.')
#                    os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
#                    return
#            else:
#                # Record statistics for samba share
#
#                if not os.path.exists(snapshot_statistics):
#                    # Statistics does not exists
#                    log.warning('Unable to read snapshot statistics. Do we have the latest S3QL?')
#                    invalidate_entry()  # Cannot finish the snapshot for some reason
#                    os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
#                    return
#
#                with open(snapshot_statistics, 'r') as fh:
#                    for lines in fh:
#                        if lines.find("total files") != -1:
#                            samba_files = int(lines.lstrip('toal fies:'))
#                        if lines.find("total size") != -1:
#                            samba_size = int(lines.lstrip('toal size:'))
#                samba_size = int(samba_size / 1000000)
#                os.system('sudo rm -rf %s' % snapshot_statistics)
#
#                cmd = "sudo python /usr/local/bin/s3qlcp %s %s/nfsshare" % (nfs_folder, temp_folder)
#                po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
#                output = po.stdout.read()
#                po.wait()
#
#                if po.returncode != 0:
#                    if output.find("Dirty cache has not been completely flushed") != -1:
#                        time.sleep(60)  # Wait one minute before retrying
#                    else:
#                        # Check if file system is still up
#                        if not os.path.exists(samba_folder):
#                            return
#
#                        invalidate_entry()
#                        log.error('Unable to finish the current snapshotting process. Aborting.')
#                        os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
#                        return
#                else:
#                    # Record statistics for NFS share
#
#                    if not os.path.exists(snapshot_statistics):
#                        # Statistics does not exists
#                        log.warning('Unable to read snapshot statistics. Do we have the latest S3QL?')
#                        invalidate_entry()  # Cannot finish the snapshot for some reason
#                        os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
#                        return
#
#                    with open(snapshot_statistics, 'r') as fh:
#                        for lines in fh:
#                            if lines.find("total files") != -1:
#                                nfs_files = int(lines.lstrip('toal fies:'))
#                            if lines.find("total size") != -1:
#                                nfs_size = int(lines.lstrip('toal size:'))
#                    nfs_size = int(nfs_size / 1000000)
#                    os.system('sudo rm -rf %s' % snapshot_statistics)
#
#                    finish = True
#        except Exception:
#            # Check if file system is still up
#            if not os.path.exists(samba_folder):
#                return
#            raise
#
#    # Begin post-processing snapshotting
#    # Update database
#    finish_time = time.time()
#    ftime_format = time.localtime(finish_time)
#    new_snapshot_name = "snapshot_%s_%s_%s_%s_%s_%s" % (ftime_format.tm_year,\
#              ftime_format.tm_mon, ftime_format.tm_mday, ftime_format.tm_hour,\
#              ftime_format.tm_min, ftime_format.tm_sec)
#    update_new_entry(new_snapshot_name, finish_time, samba_files + nfs_files,\
#              samba_size + nfs_size)
#    
#    # wthung, 2012/7/23
#    # auto expose this new created snapshot
#    snapshot.expose_snapshot(new_snapshot_name, True)
#
#    # Make necessary changes to the directory structure and lock the snapshot
#    if not os.path.exists(snapshot.snapshot_dir):
#        os.system('sudo mkdir %s' % snapshot.snapshot_dir)
#    new_snapshot_path = os.path.join(snapshot.snapshot_dir, new_snapshot_name)
#    os.system('sudo mv %s %s' % (temp_folder, new_snapshot_path))
#    os.system('sudo chown %s %s' % (samba_user, new_snapshot_path))
#    os.system('sudo python /usr/local/bin/s3qllock %s' % (new_snapshot_path))
#    os.system('sudo python /usr/local/bin/s3qlctrl upload-meta %s' % mount_point)
#    os.system('sudo rm -rf %s' % snapshot.snapshot_tag)
#    
#
#    log.debug('Snapshotting finished at %s' % str(ftime_format))

################################################################

if __name__ == '__main__':
    try:
        execute_take_snapshot()
    except Exception as err:
        log.error('%s' % str(err))
