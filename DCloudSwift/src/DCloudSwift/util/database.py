import os
import sqlite3
import util
import time
from contextlib import contextmanager

BROKER_TIMEOUT = 25

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
        except sqlite3.DatabaseError:
            try:
                conn.close()
            except:
                pass
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
		password TEXT NOT NULL, 
		admin BOOLEAN NOT NULL,
		reseller BOOLEAN NOT NULL,
		PRIMARY KEY (account, name)
            );
        """)

    def get_password(self, account, name):
	"""
        Return password of user account:name

        :param account: account of the user
	:param name: name of the user
        
	:returns: the password or None if the user doesn't exist
	"""
	with self.get() as conn:
		row = conn.execute("SELECT * FROM user_info where account=? AND name=?", (account, name)).fetchone()
		if row:
			return row["password"]
		else:
			return None

if __name__ == '__main__':

	os.system("rm /etc/test/test.db")
	db = AccountDatabaseBroker("/etc/test/test.db")
	db.initialize()

	with db.get() as conn:
		try:
			conn.execute("insert into user_info values ('system', 'root', 'testpass', 'True', 'False')")
			conn.commit()
		except Exception as e:
			print e

	print db.get_password("system", "root")
	print db.get_password("system1", "root")

	pass	
