import sys
import MySQLdb

print("Usage: python create_DB_tables.py")
DB_NAME = 'Swift_Benchmark'
DB_SERVER = 'localhost'
DB_USER = "db_client"
DB_PW = "deltacloud"

# create database if not exists
db = MySQLdb.connect(host=DB_SERVER, user=DB_USER, passwd=DB_PW, db="mysql")
cursor = db.cursor()
qstr = "CREATE DATABASE IF NOT EXISTS " + DB_NAME
cursor.execute(qstr)

# connect to database
db = MySQLdb.connect(host=DB_SERVER, user=DB_USER, passwd=DB_PW, db=DB_NAME)
cursor = db.cursor()

#~ # define parameters
#~ dest_dir = sys.argv[1]   # temporary directory for file creation, and read back.

## create a container list table (container will be randomlly generated when writting a file)
qstr = "CREATE TABLE IF NOT EXISTS Container_List(created_at TIMESTAMP, path TEXT)"
cursor.execute(qstr)
#~ qstr1 = "INSERT INTO Folder_List SET path=%s"   # insert first path
#~ cursor.execute(qstr1, dest_dir)

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
