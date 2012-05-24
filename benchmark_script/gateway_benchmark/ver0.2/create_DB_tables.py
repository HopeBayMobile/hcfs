import sys
import MySQLdb

print("Usage: ")
DB_NAME = 'Gateway_Benchmark'
DB_SERVER = 'localhost'

db = MySQLdb.connect(host=DB_SERVER, user="db_client", passwd="deltacloud", db=DB_NAME)
cursor = db.cursor()

# define parameters
dest_dir = sys.argv[1]   # temporary directory for file creation, and read back.

## create a folder list table
qstr = "CREATE TABLE IF NOT EXISTS Folder_List(created_at TIMESTAMP, path TEXT)"
cursor.execute(qstr)
qstr1 = "INSERT INTO Folder_List SET path=%s"   # insert first path
cursor.execute(qstr1, dest_dir)

## create a file list table
qstr = "CREATE TABLE IF NOT EXISTS File_List(created_at TIMESTAMP, file_name TEXT, path TEXT, file_size INT)"
cursor.execute(qstr)

## create Operation_Log table
qstr = "CREATE TABLE IF NOT EXISTS Operation_Log(action VARCHAR(64), start_time DATETIME, \
elapsed_time DOUBLE, file_size INT, success BOOLEAN, verify_checksum BOOLEAN, note TEXT)"
cursor.execute(qstr)

# close connection
cursor.close()
db.commit()
db.close()
