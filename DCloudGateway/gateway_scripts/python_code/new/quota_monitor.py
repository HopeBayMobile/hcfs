"""
This function is part of Delta Cloud Storage Gateway API functions
Developed by CTBU, Delta Electronics Inc., 2012

This source code implements the API functions for the monitor of quota
features of Delta Cloud Storage Gateway.
"""
import os
import sqlite3
import time
import subprocess
import ConfigParser
import posix_ipc
from signal import signal, SIGTERM, SIGINT
from gateway import quota_db
from gateway import common
CLOUD_ROOT_FOLDER = '/mnt/cloudgwfiles'

log = common.getLogger(name="QUOTA_MONITOR", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

g_program_exit = False

def signal_handler(signum, frame):
    """
    SIGTERM signal handler.
    Will set global flag to True to exit program safely
    """

    global g_program_exit
    g_program_exit = True
    
#def _get_folder_checklist(fetchall=False):
#    """
#    Get share folder check list from quota db.
#    Here only the changed flag == 1 will be selected into check list.
#    
#    @rtype: JSON object
#    @return: Check list of shared folder
#        - result: True if quota db exists. Otherwise, return false
#        - shared_folders: Array of shared folders which their disk usage needs to be checked.
#    """
#    ret_val = {
#               "result": False,
#               "data": {}}
#    
#    # if quota db doesn't exists, scan root of shared folder to find any sub-folder
#    # if anyone was found, try to rebuild quota db
#    if not quota_db.is_exist():
#        # rebuild quota db
#        quota_db.rebuild(quota_db.SHARED_FOLDER_ROOT_FOLDER)
#    
#    if quota_db.is_exist():
#        # per-gw quota
#        try:
#            # get check list
#            if fetchall:
#                check_list = quota_db.get('SELECT name FROM quota')
#            else:
#                check_list = quota_db.get('SELECT name FROM quota WHERE changed=1')
#            ret_val["result"] = True
#            ret_val["data"] = check_list
#        except Exception as e:
#            log.error(str(e))
#            print str(e)
#    
#    return ret_val

def _run_subprocess(cmd):
    """
    Utility function to run a command by subprocess.Popen
    
    @type cmd: string
    @param cmd: Command to run
    @rtype: tuple
    @return: Command return code and output string in a tuple
    """
    po = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = po.stdout.read()
    po.wait()
    
    return (po.returncode, output)

def _get_gateway_quota():
    """
    Get gateway quota by swift command
    
    @rtype: integer
    @return: -1 if fail. A integer larger than 0 if success. (Unit: byte)
    """
    storage_url, account, password = _get_storage_info()
    if storage_url and account and password:
        _, username = account.split(':')
        ret_code, output = _run_subprocess('sudo swift -A https://%s/auth/v1.0 -U %s -K %s stat %s_private_container' 
                                           % (storage_url, account, password, username))
        if not ret_code:
            lines = output.split('\n')
            for line in lines:
                if 'Meta Quota' in line:
                    _, meta_quota = line.split(':')
                    meta_quota = int(meta_quota)
                    return meta_quota 
        else:
            log.error('Failed to get gateway quota by swift: %s' % output)

    return -1
    #return (5 * 1024 ** 3)

def _get_storage_info():
    """
    Get storage URL and user name from /root/.s3ql/authinfo2.

    @rtype: tuple
    @return: Storage URL and user name or None if failed.
    """
    storage_url = None
    account = None
    password = None

    try:
        config = ConfigParser.ConfigParser()
        with open('/root/.s3ql/authinfo2') as op_fh:
            config.readfp(op_fh)

        section = "CloudStorageGateway"
        storage_url = config.get(section, 'storage-url').replace("swift://", "")
        account = config.get(section, 'backend-login')
        password = config.get(section, 'backend-password')
        
    except Exception as e:
        log.error("Failed to get storage info: %s" % str(e))
    finally:
        pass
    return (storage_url, account, password)

def _get_s3ql_db_name():
    """
    Get S3QL metadata db name
    
    @rtype: string
    @return: Name of S3QL metadata db, which should under /root/.s3ql.
             Return None if no S3QL db was found
    """    
    db_list = []
    dbname = None
    
    for filename in os.listdir('/root/.s3ql/'):
        if '.db' in filename:
            db_list.append(filename)
    
    if len(db_list) == 1:
        dbname = db_list[0]
    elif len(db_list) > 1:
        storage_url, account, _ = _get_storage_info()
        _, username = account.split(':')
        for db in db_list:
            if (storage_url in db) and (username in db):
                dbname = db
                break

    return dbname
    #return "s3qltmp.db"

#def _get_all_shared_folder():
#    """
#    Get all shared folder name
#    
#    @rtype: array
#    @return: Array of shared folders
#    """
#    slist = []
#    result = _get_folder_checklist(True)
#    print result
#        
#    if result["result"]:
#        for folder in result["data"]:
#            slist.append(folder[0])
#            
#    return slist

def _get_inode_subfolders(dbname, inode=1):
    """
    Get subfolder of an input inode
    
    @type dbname: string
    @param dbname: Database name
    @type inode: integer
    @param inode: Inode number
    @rtype: A list with a tuple in each element
    @return: A folder name and inode number tuple list
    """
    conn = sqlite3.connect(dbname)
    c = conn.cursor()
    query_str = 'SELECT c.inode, n.name, c.parent_inode, i.size \
            FROM names as n, contents as c, inodes as i \
            WHERE c.name_id = n.id \
            AND c.parent_inode = %d \
            AND c.inode = i.id \
            ORDER BY i.id' % inode
    c.execute(query_str)
    records = c.fetchall()
    mylist = []
    append = mylist.append
    for record in records:
        append((record[1], int(record[0])))

    conn.commit()
    c.close()
    return mylist


def _scan_subfolders(dbname, inode, folder_name):
    """
    Scan subfolders of an input inode.
    If one subfolder doesn't exist in quota DB, store it and set changed bit to 1.
    This function also cleans up database entries, which does not really exist in file system 
    
    @type dbname: string
    @param dbname: Database name
    @type inode: integer
    @param inode: Inode number
    @type folder_name: string
    @param folder_name: Folder name of input inode
    @rtype: A list with a tuple in each element
    @return: A folder name and inode number tuple list
    """
    fullpath_list = []
    dbpath_list = []
    fullpath_append = fullpath_list.append
    dbpath_append = dbpath_list.append
    
    folder_list = _get_inode_subfolders(dbname, inode)
    for folder, _ in folder_list:
        full_path = "{0}/{1}/{2}".format(quota_db.CLOUD_ROOT_FOLDER, folder_name, folder)
        fullpath_append(full_path)
        
        # check if this folder does already exist in quota db
        if not quota_db.is_dup(full_path):
            # insert record
            quota_db.insert(full_path, 0)
            # update changed bit to 1
            quota_db.update(full_path, 1, 'changed')
    
    # clean up entries which does not really exist in file system 
    data = quota_db.get('SELECT name FROM quota')
    for path in data:
        dbpath_append(path[0])
    
    for dbpath in dbpath_list:
        if dbpath not in fullpath_list:
            quota_db.delete(dbpath)
    
    return folder_list

#def _get_dataset(dbname):
#    """
#    Get while dataset we required from s3ql db
#    
#    @type dbname: string 
#    @param dbname: S3QL DB name
#    @rtype: Two-dimensional array
#    @return: Return the dataset which can be used to build the file structure
#    """
#    result_set = None
#    # connect to tmp db
#    conn = sqlite3.connect(dbname)
#    c = conn.cursor()
#    c.execute('SELECT c.inode, n.name, c.parent_inode, i.size \
#            FROM names as n, contents as c, inodes as i \
#            WHERE c.name_id = n.id \
#            AND c.inode = i.id \
#            ORDER BY i.id')
#    records = c.fetchall()
##    for record in records:
##        print record[0] # inode
##        print record[1] # name
##        print record[2] # parent inode
##        print record[3] # size
#        
#        
#    
#    conn.commit()
#    c.close()
#    
#    return records

def _dup_s3ql_db():
    """
    Duplicate S3QL database to memory
    
    @rtype: string
    @param: Full path of copied database
    """
    # copy s3ql db to /dev/shm
    db_name = _get_s3ql_db_name()
    target_db = '/dev/shm/s3qltmp.db'
    os.system('sudo rm %s' % target_db)
    os.system('sudo cp /root/.s3ql/%s %s' % (db_name, target_db))
    return target_db

def _has_child(inode, c):
    """
    Check if an inode has children
    
    @type inode: integer
    @param inode: inode number
    @type c: Database cursor
    @param c: Database cursor to do query
    @rtype: boolean
    @return: True if input inode has at least a child. Otherwise, return False
    """
    query_str = 'SELECT 1 FROM contents WHERE parent_inode = %d' % inode
    c.execute(query_str)
    if c.fetchone() is not None:
        return True
    return False

def _get_inode_usage(inode, c):
    """
    Get disk usage of a given inode
    
    @type inode: integer
    @param inode: inode number
    @type c: Database cursor
    @param c: Database cursor to do query
    @rtype: integer
    @return: Disk usage of input inode
    """
    query_str = 'SELECT c.inode, n.name, c.parent_inode, i.size \
            FROM names as n, contents as c, inodes as i \
            WHERE c.name_id = n.id \
            AND c.parent_inode = %d \
            AND c.inode = i.id \
            ORDER BY i.id' % inode
    c.execute(query_str)
    records = c.fetchall()
    
    has_child = False
    usage = 0
    for record in records:
        size = int(record[3])
        child_inode = int(record[0])
        usage += size
        if _has_child(child_inode, c):
            # recursive call
            usage += _get_inode_usage(child_inode, c)
        
    return usage

def _get_usage(dbname, inode):
    """
    Get disk usage of a input inode
    
    @type dbname: string
    @param dbname: Quota database name
    @type inode: integer
    @param inode: Inode number
    @rtype: integer
    @return: Disk usage of input inode
    """    
    conn = sqlite3.connect(dbname)
    c = conn.cursor()
    
    usage = _get_inode_usage(inode, c)
    #print("Get usage, id={0}, usage={1}".format(inode, usage))
    
    conn.commit()
    c.close()
    
    return usage

def _get_usage_and_update(dbname, inode, folder_name):
    """
    Get disk usage of a input inode and update quota db
    
    @type dbname: string
    @param dbname: Quota database name    
    @type inode: integer
    @param inode: Inode number
    @rtype: integer
    @return: Disk usage of input inode
    """
    total_usage = 0
    folders = _scan_subfolders(dbname, inode, folder_name)
    for folder, inode in folders:
        # combine the full path string
        full_path = "{0}/{1}/{2}".format(quota_db.CLOUD_ROOT_FOLDER, folder_name, folder)
        #print('Checking %s' % full_path)
        # check if files under this path is not changed
        if quota_db.is_chaged_bit(full_path):
            # get disk usage of this inode by scanning s3ql db
            #print 'Get usage by scanning s3ql db'
            usage = _get_usage(dbname, inode)
            # update to quota db
            quota_db.update(full_path, usage, 'usage')
            quota_db.update(full_path, 0, 'changed')
        else:
            #print 'Get existing usage'
            query_str = "SELECT usage FROM quota WHERE name='%s'" % full_path
            data = quota_db.get(query_str)
            usage = int(data[0][0])
        
        total_usage += usage
    
    return total_usage

#def _is_over_quota(folder, usage):
#    """
#    Check if the disk usage of a folder is over the quota
#    
#    @type folder: string
#    @param folder: Shared folder name
#    @type usage: integer
#    @param usage: Disk usage of shared folder
#    @rtype: boolean
#    @return: True if over quota, otherwise False
#    """
#    quota_val = quota_db.get('SELECT quota FROM quota WHERE name=%s' % folder)
#    if quota_val > usage:
#        return True
#    
#    return False

def _is_over_gateway_quota(current_usage):
    """
    Check if cloud folder is over the whole quota
    
    @type current_usage: integer
    @param current_usage: Current disk usage of whole gateway
    @rtype: boolean
    @return: True if over quota. Otherwise, False
    """
    whole_quota = _get_gateway_quota()
    #print("Whole GW quota: %d" % whole_quota)
    
    if whole_quota <= 0:
        # default quota 5G
        while_quota = 5 * 1024 ** 3
        log.debug('Use 5G as default quota')
    
    log.debug("Total usage/quota: %d/%d" % (current_usage, whole_quota))
    
    if whole_quota < current_usage:
        return True
    
    return False
    

def _lock_root_folder(unlock=False):
    """
    Lock/unlock s3ql mount folder by s3qlctrl writeon/writeoff
        
    @type unlock: boolean
    @param unlock: Unlock operation
    """
    if unlock:
        cmd = 'sudo s3qlctrl writeon %s' % CLOUD_ROOT_FOLDER
        log.debug('Unlock S3QL writing')
    else:
        cmd = 'sudo s3qlctrl writeoff %s' % CLOUD_ROOT_FOLDER
        log.debug('Lock S3QL writing')
    
    try:
        po = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = po.stdout.read()
        po.wait()
        
        if po.returncode != 0:
            log.error('Failed to lock/unlock %s. Unlock flag=%d' % (fullpath, unlock))
    except Exception as e:
        log.error(str(e))

def _is_fschg_notifier_started():
    """
    Check if our file system change notifier was started
    
    @rtype: boolean
    @return: True if started. Otherwise, False
    """
    try:
        mq = posix_ipc.MessageQueue(quota_db.MQ_NAME, flags=posix_ipc.O_CREAT)
        mq.send('?monitor_started', 1)
        s, _ = mq.receive()
        s = s.decode()
        if s == '!started':
            return True
    except Exception as e:
        log.error(str(e))
    
    return False

def _upload_usage_data(usage):
    """
    Upload gateway usage to swift
    
    @type usage: integer
    @param usage: Gateway usage
    """
    target_file = 'gw_total_usage'
    os.system('sudo echo %d > %s' % (usage, target_file))
    storage_url, account, password = _get_storage_info()
    if storage_url and account and password:
        _, username = account.split(':')
        _, output = _run_subprocess('sudo swift -A https://%s/auth/v1.0 -U %s -K %s upload %s_private_container %s' 
                                           % (storage_url, account, password, username, target_file))
        if target_file in output:
            log.info('Uploaded gateway total usage to swift (%d)' % usage)
        else:
            log.error('Failed to upload gateway total usage to swift. %s' % output)

def main():
    """
    Main entry point.
    """
    global g_program_exit
    # todo: check arguments

    # daemonize myself
    common.daemonize()

    # normal exit when killed
    signal(SIGTERM, signal_handler)
    signal(SIGINT, signal_handler)
    
    # check if quota db exists
    if not quota_db.is_exist():
        quota_db.create()
        
    is_fschg_notifier_start = False
    sleep_time = 120

    while not g_program_exit: 
        #result = _get_folder_checklist()
        #print result
        
        #print("Wait %d seconds..." % sleep_time)
        # sleep for some time by a for loop in order to break at any time
        for _ in range(sleep_time):
            time.sleep(1)
            if g_program_exit:
                return        
        
        if not is_fschg_notifier_start:
            # check if fschg_notifier is started
            if _is_fschg_notifier_started():
                is_fschg_notifier_start = True
                log.debug('fschg notifier started')
            else:
                log.debug('fschg notifier not started')
        
        start_time = time.time()
        dbname = _dup_s3ql_db()
        #print("Copied database, spend %d seconds" % (time.time() - start_time))
        
        start_time = time.time()
        root_folder_list = _get_inode_subfolders(dbname, 1)
        
        total_usage = 0
        nfs_usage = 0
        samba_usage = 0
        
        for folder, inode in root_folder_list:
            if is_fschg_notifier_start:
                # check nfsshare and sambashare separately
                if folder == u'nfsshare':
                    # calculate subfolders of nfsshare
                    nfs_usage = _get_usage_and_update(dbname, inode, 'nfsshare')
                elif folder == u'sambashare':
                    # calculate subfolders of sambashare
                    samba_usage = _get_usage_and_update(dbname, inode, 'sambashare')
                else:
                    # calculate other subfolders
                    #print("Checking %s" % folder)
                    total_usage += _get_usage(dbname, inode)
            else:
                # fschg notifier is not started, we have to get usage by scanning s3ql db
                #print("Checking %s" % folder)
                total_usage += _get_usage(dbname, inode)
        
        dt = time.time() - start_time
        #print("Got usage of all folders, spend %d seconds" % dt)
        
        # decide next sleeping time, 30 < sleeping time < 600
        if dt > sleep_time:
            sleep_time = min(int(sleep_time * 2), 600)
        elif dt < (sleep_time / 3):
            sleep_time = max(int(sleep_time / 2), 120)
        
        # add nfs and samba usage to total usage
        total_usage += nfs_usage
        total_usage += samba_usage
        
#        if result["result"]:
#            # check per-folder quota
#            for folder in result["data"]:
#                disk_usage = _get_usage(folder)
#                quota_db.update(folder, disk_usage, "usage")
#                # todo: need to check if it's ok to lock/unlock multiple times
#                if _is_over_quota(folder, disk_usage):
#                    _lock_folder(folder)
#                else:
#                    _lock_folder(folder, True)

        # check whole gw quota
        if _is_over_gateway_quota(total_usage):
            _lock_root_folder(False)
        else:            
            _lock_root_folder(True)
        
        # upload current usage to swift user container
        _upload_usage_data(total_usage)
        

if __name__ == '__main__':
    #dbname = _dup_s3ql_db()
    #print _get_usage(dbname, 1)
    main()