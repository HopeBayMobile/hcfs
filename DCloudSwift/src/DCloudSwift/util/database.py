import os
import sqlite3
import util
import time
from contextlib import contextmanager

BROKER_TIMEOUT = 60

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

class AccountDatabaseBroker(DatabaseBroker):
    """Encapsulates working with a Account database."""

    def _initialize(self, conn):
        self.create_user_info_table(conn)

    def create_user_info_table(self, conn):
        """
        Create user_info table which is specific to the account DB.

        :param conn: DB connection object
        """
	
        conn.executescript("""
            CREATE TABLE user_info (
                account TEXT NOT NULL,
                name TEXT NOT NULL,
                enabled BOOLEAN NOT NULL,
                PRIMARY KEY (account, name)
            );
        """)
    def show_user_info_table(self):
        with self.get() as conn:
            row = conn.execute("SELECT * FROM user_info").fetchall()
            return row
 
    def add_user(self, account, name):
        """
        add user into the db

        :param account: account of the user
        :param name: name of the user
        
        :returns: return None if the user already exists. Otherwise return the newly added row
        """
        with self.get() as conn:
            row_account = conn.execute("SELECT * FROM user_info where account=?", (account,)).fetchone()
            row_name = conn.execute("SELECT * FROM user_info where account=? AND name=?", (account, name)).fetchone()        
            
            if row_name:
                return None          
            if not row_account:
                return False
            else:
                conn.execute("INSERT INTO user_info VALUES (?,?,?)", (account, name, 1))
                conn.commit()
                row = conn.execute("SELECT * FROM user_info where account=? AND name=?", (account, name)).fetchone()
                return row

    def delete_user(self, account, name):
        '''
         delete user from the db

        :param account: account of the user    
        :param name: name of the user     
        :returns: return None if the user does not exist; otherwise return the deleted row
        '''
        with self.get() as conn:
            row = conn.execute("SELECT * FROM user_info where account=? AND name=?", (account, name)).fetchone()
            if row:
                conn.execute("DELETE FROM user_info WHERE account=? AND name=?", (account, name))
                conn.commit()
                return row
            else:
                return None

    def add_account(self, account):
        """
        add account into the db
        @author: Rice
        
        @type  account: string
        @param account: the name of account
        @return: return None if the account already exists. Otherwise return the newly added row.
        """      

        with self.get() as conn:
            row = conn.execute("SELECT * FROM user_info where account=?", (account,)).fetchone()
            if row:
                return None
            else:
                conn.execute("INSERT INTO user_info VALUES (?,0,?)", (account, 1))
                conn.commit()
                row = conn.execute("SELECT * FROM user_info where account=?", (account,)).fetchone()
                return row

    def delete_account(self, account):
        '''
         delete user from the db
         @author: Rice
         
         @type  account: string
         @param account: the name of account
         @return: return None if the account does not exists. Otherwise, return the deleted row.
        '''
        
        with self.get() as conn:
            row = conn.execute("SELECT * FROM user_info where account=?", (account,)).fetchone()
            if row:
                conn.execute("DELETE FROM user_info WHERE account=?", (account,))
                conn.commit()
                return row
            else:
                return None

    def disable_user(self, account, name):
	"""
         set enabled of user to False if the user exists

        :param account: account of the user
	:param name: name of the user
        
	:returns: no return
	"""
	with self.get() as conn:
		conn.execute("UPDATE user_info SET enabled = ? where account=? AND name=?", (0, account, name))
		conn.commit()

    def enable_user(self, account, name):
	"""
         set enabled of user to True if the user exists

        :param account: account of the user
	:param name: name of the user
        
	:returns: no return
	"""
	with self.get() as conn:
		conn.execute("UPDATE user_info SET enabled = ? where account=? AND name=?", (1, account, name))
		conn.commit()

    def is_enabled(self, account, name):
	"""
        check if user account:name is enabled

        :param account: account of the user
	:param name: name of the user
        
	:returns: if account:name exists and is enabled then return true. Otherwise return false.
	"""
	with self.get() as conn:
		row = conn.execute("SELECT * FROM user_info where account=? AND name=?", (account, name)).fetchone()
		if row:
			if row["enabled"] == 1:
				return True
			else:
				return False
		else:
			return False
			

if __name__ == '__main__':

	os.system("rm /etc/test/test.db")
	db = AccountDatabaseBroker("/etc/test/test.db")
	db.initialize()

	print db.add_user(account="system", name="root", password="testpass", admin=True, reseller=False)
	print db.add_user(account="system", name="root", password="pass", admin=False, reseller=True)
	print db.delete_user(account="system", name="root")
	print db.add_user(account="system", name="root", password="pass", admin=False, reseller=True)

	print db.get_password("system", "root")
	print db.get_password("system1", "root")
	print db.is_enabled("system", "root")
	print db.is_admin("system","root")
	print db.is_reseller("system","root")
	#db.change_password("system", "root",None)
	print db.get_password("system", "root")
	db.disable_user("system", "root")
	print db.is_enabled("system", "root")
	pass	
