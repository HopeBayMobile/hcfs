"""
This function is part of Delta Cloud Storage Gateway API functions
Developed by CTBU, Delta Electronics Inc., 2012

This source code implements the API functions for the db operation of quota
features of Delta Cloud Storage Gateway.
"""

import os
import common
import sqlite3

log = common.getLogger(name="QUOTA_DB", conf="/etc/delta/Gateway.ini")
DIR = os.path.dirname(os.path.realpath(__file__))

DB_NAME = '/root/.s3ql/quota.db'
CLOUD_ROOT_FOLDER = '/mnt/cloudgwfiles'
MQ_NAME = '/DCloudGatewayQuotaMessageQueue'
#SHARED_FOLDER_ROOT_FOLDER = '%s/shared_root' % CLOUD_ROOT_FOLDER

class QuotaError(Exception):
    pass

def _connect():
    try:
        conn = sqlite3.connect(DB_NAME)
        c = conn.cursor()
    except Exception as e:
        log.error(str(e))
    
    return (conn, c)

def create():
    """
    Create a quota database with a table named quota, which name column is unique
    """
    try:
        conn, c = _connect()
        c.execute('''CREATE TABLE quota (
            name TEXT UNIQUE NOT NULL, 
            quota INTEGER NOT NULL, 
            usage INTEGER NOT NULL DEFAULT 0, 
            changed BOOLEAN NOT NULL DEFAULT 0
            )''')
        conn.commit()
        log.debug("%s has been created" % DB_NAME)
    except Exception as e:
        log.error(str(e))
    finally:
        c.close()

def is_exist():
    """
    Check if quota db does exist
    
    @rtype: boolean
    @return: True if db exists, otherwise false
    """
    if os.path.exists(DB_NAME):
        return True
    return False

def is_dup(name):
    """
    Check if a name is already in the database
    
    @type name: string
    @param name: shared folder name
    @rtype: boolean
    @return: True if shared folder exists in db, otherwise false
    """
    ret = False
    try:
        conn, c = _connect()
        c.execute('SELECT 1 FROM quota WHERE name=?', (name,))
        if c.fetchone() is not None:
            ret = True
        conn.commit()
    except Exception as e:
        log.error(str(e))
    finally:
        c.close()

    return ret

def is_chaged_bit(name):
    """
    Check if the changed bit of input name is 1 in database
    
    @type name: string
    @param name: shared folder name
    @rtype: boolean
    @return: True if changed bit is 1, otherwise false
    """
    ret = False
    try:
        conn, c = _connect()
        c.execute('SELECT 1 FROM quota WHERE name=? AND changed=1', (name,))
        if c.fetchone() is not None:
            ret = True
        conn.commit()
    except Exception as e:
        log.error(str(e))
    finally:
        c.close()

    return ret

def rebuild(folder):
    """
    Rebuild quota db by scanning input folder
    
    @type folder: string
    @param folder: Folder name to scan for rebuilding quota db
    """
    # default quota
    default_quota = 20 * (1024 ** 3)
    
    # delete quota db if exists
    if os.path.exists(DB_NAME):
        os.system('rm -rf %s' % DB_NAME)
    
    # create db
    create()
    
    item_list = os.listdir(folder)
    for item in item_list:
        if os.path.isdir(folder + '/' + item):
            #print('Found %s in %s' % (item, SHARED_FOLDER_ROOT_FOLDER))
            insert(item, default_quota)

def get(query=None):
    """
    Get db data by input query string.
    If no query string input, return all db data.
    
    @type query: string
    @param query: SQL query string
    @rtype: result set
    @return: An result set meets input query
    """
    query_str = None
    result_set = None
    
    if not query:
        query_str = 'SELECT * FROM quota'
    else:
        query_str = query
        
    try:
        conn, c = _connect()
        c.execute(query_str)
        result_set = c.fetchall()
    except Exception as e:
        log.error(str(e))
    finally:
        c.close()
    
    return result_set

def insert(name, quota):
    """
    Insert a new quota to database
    
    @type name: string
    @param name: Shared folder name
    @type quota: integer
    @param quota: Quota value in bytes
    """
    #print('insert new quota')
    try:
        conn, c = _connect()
        c.execute('INSERT INTO quota(name,quota) VALUES(?,?)', (name, quota))
        conn.commit()
    except Exception as e:
        log.error(str(e))
    finally:
        c.close()

def delete(name):
    """
    Delete a record by name in database
    
    @type name: string
    @param name: shared folder name
    """
    try:
        conn, c = _connect()
        c.execute('DELETE FROM quota WHERE name=?', (name,))
        if c.fetchone() is not None:
            ret = True
        conn.commit()
    except Exception as e:
        log.error(str(e))
    finally:
        c.close()

def update(name, value, ctype):
    """
    Update existing quota
    
    @type name: string
    @param name: Shared folder name
    @type value: integer
    @param value: Value to be set
    @type ctype: string
    @param ctype: The type to set. Ex: quota, usage, changed
    """
    #print('update quota')
    try:
        conn, c = _connect()
        if ctype == 'quota':
            c.execute('UPDATE quota SET quota=? WHERE name=?', (value, name))
        elif ctype == 'usage':
            c.execute('UPDATE quota SET usage=? WHERE name=?', (value, name))
        elif ctype == 'changed':
            c.execute('UPDATE quota SET changed=? WHERE name=?', (value, name))
        else:
            raise QuotaError('Wrong quota type')
        conn.commit()
    except Exception as e:
        log.error(str(e))
    finally:
        c.close()
