import os
import sqlite3
import util
import time
import json
import fcntl
import functools
from contextlib import contextmanager

BROKER_TIMEOUT = 60

# lock decorator
def lock(fn):
    @functools.wraps(fn)
    def wrapper(self, *args, **kwargs):
        handle = open(self.lockfile, 'w')
        try:
            fcntl.flock(handle, fcntl.LOCK_EX)
            ret = fn(self, *args, **kwargs)
            return ret
        finally:
            fcntl.flock(handle, fcntl.LOCK_UN)
            handle.close()

    return wrapper  # decorated function



class DatabaseConnectionError(sqlite3.DatabaseError):
    """More friendly error messages for DB Errors."""

    def __init__(self, path, msg, timeout=0):
        self.path = path
        self.timeout = timeout
        self.msg = msg

    def __str__(self):
        return 'DB connection error (%s, %s):\n%s' % (
                self.path, self.timeout, self.msg)


def get_db_connection(path, timeout=30, okay_to_create=False):
    """
    Returns a properly configured SQLite database connection.

    :param path: path to DB
    :param timeout: timeout for connection
    :param okay_to_create: if True, create the DB if it doesn't exist
    :returns: DB connection object
    """
    try:
        connect_time = time.time()
        conn = sqlite3.connect(path, check_same_thread=False, timeout=timeout)

        if path != ':memory:' and not okay_to_create:
            # attempt to detect and fail when connect creates the db file
            stat = os.stat(path)
            if stat.st_size == 0 and stat.st_ctime >= connect_time:
                os.unlink(path)
                raise DatabaseConnectionError(path,
                    'DB file created by connect?')
        conn.row_factory = sqlite3.Row
        conn.text_factory = str
        conn.execute('PRAGMA synchronous = NORMAL')
        conn.execute('PRAGMA count_changes = OFF')
        conn.execute('PRAGMA temp_store = MEMORY')
        conn.execute('PRAGMA journal_mode = DELETE')
    except sqlite3.DatabaseError:
        import traceback
        raise DatabaseConnectionError(path, traceback.format_exc(),
                timeout=timeout)
    return conn


class DatabaseBroker(object):
    """Encapsulates working with a database."""

    def __init__(self, db_file, timeout=BROKER_TIMEOUT):
        """ Encapsulates working with a database. """
        self.conn = None
        self.db_file = db_file
        self.db_dir = os.path.dirname(db_file)
        self.timeout = timeout
        self.lockfile = "/etc/delta/lockfiles" + db_file
        
        lockfile_dir = os.path.dirname(self.lockfile)
        if not os.path.exists(lockfile_dir):
            util.mkdirs(lockfile_dir)

    def initialize(self):
        """
        Create the DB
        """
        conn = None
        if self.db_file == ':memory:':
            conn = get_db_connection(self.db_file, self.timeout)
        else:
            util.mkdirs(self.db_dir)
            conn = sqlite3.connect(self.db_file, check_same_thread=False, timeout=0)

        conn.row_factory = sqlite3.Row
        conn.text_factory = str
        self._initialize(conn)
        conn.commit()
        self.conn = conn

    @contextmanager
    def get(self):
        """Use with the "with" statement; returns a database connection."""
        if not self.conn:
            if self.db_file != ':memory:' and os.path.exists(self.db_file):
                    self.conn = get_db_connection(self.db_file, self.timeout)
            else:
                raise DatabaseConnectionError(self.db_file, "DB doesn't exist")

        conn = self.conn
        self.conn = None
        try:
            yield conn
            conn.rollback()
            self.conn = conn
        except Exception:
            conn.close()
            raise


class NodeInfoDatabaseBroker(DatabaseBroker):
    """Encapsulates working with a node information database."""


    def add_info_and_spec(self, nodeList):
        """
        add info and specs for nodes not in node list
        """
        for node in nodeList:
            hostname = node["hostname"]
            status = "alive"
            timestamp = int(time.time())

            disk_info = {
                        "timestamp": timestamp,
                        "missing": {"count": 0, "timestamp": timestamp},
                        "broken": [],
                        "healthy": [],
            }
            disk = json.dumps(disk_info)

            daemon_info = {
                        "timestamp": timestamp,
                        "on": [],
                        "off": [],
            }
            daemon = json.dumps(daemon_info)

            mode = "service"
            switchpoint = timestamp

            self.add_node(hostname=hostname,
                          status=status,
                          timestamp=timestamp,
                          disk=disk,
                          daemon=daemon,
                          mode=mode,
                          switchpoint=switchpoint)

            self.add_spec(hostname=hostname, diskcount=node["deviceCnt"], diskcapacity=node["deviceCapacity"])

    def constructDb(self, nodeList=None):
        """
        construct db from node list
        """

        if os.path.exists(self.db_file):
            code = os.system("rm %s" % self.db_file)
            if code != 0:
                raise Exception("Failed to remove %s" % self.db_file)

        self.initialize()
        self.add_info_and_spec(nodeList)

    def _initialize(self, conn):
        self.create_node_info_table(conn)
        self.create_node_spec_table(conn)

    def create_node_info_table(self, conn):
        """
        Create node information table which is specific to the DB.
        
        @type  conn: object
        @param conn: DB connection object
        """
        conn.executescript("""
            CREATE TABLE node_info (
                hostname TEXT NOT NULL,
                status TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                disk TEXT NOT NULL,
                daemon TEXT NOT NULL,
                mode TEXT NOT NULL,
                switchpoint INTEGER NOT NULL,
                PRIMARY KEY (hostname)
            );
        """)

    def create_node_spec_table(self, conn):
        """
        Create node information table which is specific to the DB.
        
        @type  conn: object
        @param conn: DB connection object
        """
        conn.executescript("""
            CREATE TABLE node_spec (
                hostname TEXT NOT NULL,
                timestamp INTEGER NOT NULL,
                diskcount INTEGER NOT NULL,
                diskcapacity INTEGER NOT NULL,
                PRIMARY KEY (hostname)
            );
        """)

    def show_node_info_table(self):
        """
        show node_info table
        
        @rtype:  string
        @return: return node_info table
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_info").fetchall()
            return row

    def show_node_spec_table(self):
        """
        show node_spec table
        
        @rtype:  rows
        @return: return node_info table
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_spec").fetchall()
            return row

    @lock
    def add_node(self, hostname, status, timestamp, disk, daemon, mode, switchpoint):
        """
        add node to node_info

        @type  hostname: string
        @param hostname: hostname of the node
        @type  status: enum(alive, unknown, dead)
        @param status: status of the node
        @type  timestamp: integer
        @param timestamp: time of the latest status update
        @type  disk: json string
        @param disk: disk information of the node
        @type  daemon: json string
        @param daemon: daemon information of the node
        @type mode: enum(service, waiting)
        @param mode: mode of the node
        @type switchpoint: integer
        @param switchpoint: time of the latest mode update
        @rtype: string
        @return: Return None if the host already exists. 
            Otherwise return the newly added row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
            if row:
                return None
            else:
                conn.execute("INSERT INTO node_info VALUES (?,?,?,?,?,?,?)", (hostname, status, timestamp, disk, daemon, mode, switchpoint))
                conn.commit()
                row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
                return row

    @lock
    def add_spec(self, hostname, diskcount, diskcapacity):
        """
        add spec to node_spec

        @type  hostname: string
        @param hostname: hostname of the node
        @type  diskcount: integer
        @param diskcount: number of disks in the node
        @type  diskcapacity: integer
        @param diskcapacity: capacity of each individual disk
        @rtype: row
        @return: Return None if the host already exists. 
            Otherwise return the newly added row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_spec where hostname=?", (hostname,)).fetchone()
            if row:
                return None
            else:
                timestamp = int(time.time())
                conn.execute("INSERT INTO node_spec VALUES (?,?,?,?)", (hostname, timestamp, diskcount, diskcapacity))
                conn.commit()
                row = conn.execute("SELECT * FROM node_spec where hostname=?", (hostname,)).fetchone()
                return row

    @lock
    def delete_node(self, hostname):
        """
        delete a node from node_info

        @type  hostname: string
        @param hostname: hostname of the node
        @rtype: string
        @return: Return None if the host does not exist. 
            Otherwise return the deleted row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
            if row:
                conn.execute("DELETE FROM node_info WHERE hostname=?", (hostname,))
                conn.commit()
                return row
            else:
                return None

    @lock
    def delete_spec(self, hostname):
        """
        delete a spec from node_spec

        @type  hostname: string
        @param hostname: hostname of the node
        @rtype: row
        @return: Return None if the host does not exist. 
            Otherwise return the deleted row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_spec where hostname=?", (hostname,)).fetchone()
            if row:
                conn.execute("DELETE FROM node_spec WHERE hostname=?", (hostname,))
                conn.commit()
                return row
            else:
                return None

    def query_node_info_table(self, conditions):
        """
        query node_info according to conditions

        @type  conditions: string
        @param conditions: query conditions
        @rtype: string
        @return: Return the result.            
        """
        with self.get() as conn:
            ret = conn.execute("SELECT * FROM node_info WHERE %s" % conditions)
            return ret
        
    def query_node_spec_table(self, conditions):
        """
        query node_info according to conditions

        @type  conditions: string
        @param conditions: query conditions
        @rtype: rows
        @return: Return the result.            
        """
        with self.get() as conn:
            ret = conn.execute("SELECT * FROM node_spec WHERE %s" % conditions)
            return ret

    def get_info(self, hostname):
        """
        retrieve information of a node from node_info
        
        @type  hostname: string
        @param hostname: hostname of the node
        @rtype: string
        @return: Return None if the host does not exist. 
            Otherwise return the seleted row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
            if not row:
                return None
            else:
                return row
 
    def get_spec(self, hostname):
        """
        retrieve information of a node from node_info
        
        @type  hostname: string
        @param hostname: hostname of the node
        @rtype: row
        @return: Return None if the host does not exist. 
            Otherwise return the seleted row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_spec where hostname=?", (hostname,)).fetchone()
            if not row:
                return None
            else:
                return row

    @lock
    def update_node_status(self, hostname, status, timestamp):
        """
        update node status information to node_info

        @type  hostname: string
        @param hostname: hostname of the node
        @type  status: enum(alive, unknown, dead)
        @param status: status of the node
        @type  timestamp: integer
        @param timestamp: date of the latest heartbeat
        @rtype: string
        @return: Return None if the host does not exist. 
            Otherwise return the newly updated row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
            if not row:
                return None
            else:
                conn.execute("UPDATE node_info SET status=?, timestamp=? WHERE hostname=?", (status, timestamp, hostname))
                conn.commit()
                row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
                return row

    @lock
    def update_node_disk(self, hostname, disk):
        """
        update disk information into node_info

        @type  hostname: string
        @param hostname: hostname of the node
        @type  disk: encoding json string
        @param disk: disk status of the node
        @rtype: string
        @return: Return None if the node does not exist. 
            Otherwise return the newly added row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
            if not row:
                return None
            else:
                conn.execute("UPDATE node_info SET disk=? WHERE hostname=?", (disk, hostname))
                conn.commit()
                row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
                return row

    @lock
    def update_node_daemon(self, hostname, daemon):
        """
        update daemon information into node_info

        @type  hostname: string
        @param hostname: hostname of the node
        @type  daemon: encoding json string
        @param daemon: daemon status of the node
        @rtype: string
        @return: Return None if the node does not exist. 
            Otherwise return the newly added row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
            if not row:
                return None
            else:
                conn.execute("UPDATE node_info SET daemon=? WHERE hostname=?", (daemon, hostname))
                conn.commit()
                row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
                return row

    @lock
    def update_node_mode(self, hostname, mode, switchpoint):
        """
        update disk mode information into node_info

        @type  hostname: string
        @param hostname: hostname of the node
        @type  mode: enum(service, waiting)
        @param mode: mode of node
        @type  switchpoint: integer
        @param switchpoint: date of the latest mode switching
        @rtype: string
        @return: Return None if the node does not exist. 
            Otherwise return the newly added row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
            if not row:
                return None
            else:
                conn.execute("UPDATE node_info SET mode=?, switchpoint=? WHERE hostname=?", (mode, switchpoint, hostname))
                conn.commit()
                row = conn.execute("SELECT * FROM node_info where hostname=?", (hostname,)).fetchone()
                return row

    @lock
    def update_spec_diskcount(self, hostname, diskcount):
        """
        update node status information to node_info

        @type  hostname: string
        @param hostname: hostname of the node
        @type  diskcount: integer
        @param diskcount: number of disks in the node
        @rtype: row
        @return: Return None if the host does not exist. 
            Otherwise return the newly updated row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_spec where hostname=?", (hostname,)).fetchone()
            if not row:
                return None
            else:
                timestamp = int(time.time())
                conn.execute("UPDATE node_spec SET diskcount=?, timestamp=? WHERE hostname=?", (diskcount, timestamp, hostname))
                conn.commit()
                row = conn.execute("SELECT * FROM node_spec where hostname=?", (hostname,)).fetchone()
                return row

    @lock
    def update_spec_diskcapacity(self, hostname, diskcapacity):
        """
        update node status information to node_info

        @type  hostname: string
        @param hostname: hostname of the node
        @type  diskcapacity: integer
        @param diskcapacity: capacity of each individual disk
        @rtype: row
        @return: Return None if the host does not exist. 
            Otherwise return the newly updated row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM node_spec where hostname=?", (hostname,)).fetchone()
            if not row:
                return None
            else:
                timestamp = int(time.time())
                conn.execute("UPDATE node_spec SET diskcapacity=?, timestamp=? WHERE hostname=?", (diskcapacity, timestamp, hostname))
                conn.commit()
                row = conn.execute("SELECT * FROM node_spec where hostname=?", (hostname,)).fetchone()
                return row


class MaintenanceBacklogDatabaseBroker(DatabaseBroker):
    """Encapsulates working with a event list database."""

    def _initialize(self, conn):
        self.create_maintenance_backlog_table(conn)

    def create_maintenance_backlog_table(self, conn):
        """
        Create maintenance_backlog which is specific to the DB.
        
        @type  conn: object
        @param conn: DB connection object
        """
        conn.executescript("""
            CREATE TABLE maintenance_backlog (
                target TEXT NOT NULL,
                hostname TEXT NOT NULL,
                disks_to_reserve TEXT,
                disks_to_replace TEXT,
                timestamp INTEGER NOT NULL,
                PRIMARY KEY (hostname)
            );
        """)

    def show_maintenance_backlog_table(self):
        """
        show maintenance_backlog table
        
        @rtype:  string
        @return: return maintenance_backlog table
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM maintenance_backlog").fetchall()
            return row

    def add_maintenance_task(self, target, hostname, disks_to_reserve, disks_to_replace):
        """
        add task to maintenance_backlog

        @type  target: enum(node_missing, disk_broken, disk_missing)
        @param target: status of the node
        @type  hostname: string
        @param hostname: hostname of the node to maintain
        @type  disks_to_reserve: json string
        @param disks_to_reserve: SN list of disks to reserve
        @type  disks_to_replace: json string
        @param disks_to_replace: SN list of disks to replace
        @rtype: string
        @return: Return None if the node already exists. 
            Otherwise return the newly added row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM maintenance_backlog where hostname=?", (hostname,)).fetchone()
            if row:
                return None
            else:
                timestamp = int(time.time())
                conn.execute("INSERT INTO maintenance_backlog VALUES (?,?,?,?,?)", (target, hostname, disks_to_reserve, disks_to_replace, timestamp))
                conn.commit()
                row = conn.execute("SELECT * FROM maintenance_backlog where hostname=?", (hostname,)).fetchone()
                return row
    def delete_maintenance_task(self, hostname):
        """
        delete task from maintenance_backlog

        @type  hostname: string
        @param hostname: hostname of the node to maintain
        @rtype: string
        @return: Return None if the node does not exist. 
            Otherwise return the newly added row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM maintenance_backlog where hostname=?", (hostname,)).fetchone()
            if row:
                conn.execute("DELETE FROM maintenance_backlog WHERE hostname=?", (hostname,))
                conn.commit()
                return row
            else:
                return None

    def query_maintenance_backlog_table(self, conditions):
        """
        query maintenance_backlog according to the conditions

        @type  conditions: string
        @param conditions: query conditions
        @rtype: string
        @return: Return the result.            
        """
        with self.get() as conn:
            ret = conn.execute("SELECT * FROM maintenance_backlog WHERE %s" % conditions)
            return ret

    def get_info(self, hostname):
        """
        retrieve information of a node from maintenance_backlog

        @type  hostname: string
        @param hostname: hostname of the node to maintain3
        @rtype: string
        @return: Return None if the host doesn't exist. 
            Otherwise return the newly added row.            
        """
        with self.get() as conn:
            row = conn.execute("SELECT * FROM maintenance_backlog where hostname=?", (hostname,)).fetchone()
            if not row:
                return None
            else:
                return row

if __name__ == '__main__':
    os.system("rm /etc/delta/swift_node.db")
    db = NodeInfoDatabaseBroker("/etc/delta/swift_node.db")
    db.initialize()
    db.add_spec(hostname="ddd", diskcount=3, diskcapacity=100)
#    db.add_spec(hostname="dd", diskcount=3)
#    rows = db.show_node_spec_table()
#    for row in rows:
#        print row
#    db.add_node(hostname="ddd", status="alive", timestamp=123, disk="{}", mode="service", switchpoint=234)
    print db.get_spec("ddd")
    print db.update_spec_diskcount("ddd", 5)
    print db.delete_spec("ddd")
    #print db.add_node(hostname="system", ipaddress=None)
    pass
