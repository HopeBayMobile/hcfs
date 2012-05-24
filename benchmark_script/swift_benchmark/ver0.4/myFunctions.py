## issue list
## 1. "a = os.system(cmd)" and "if a>0:" cannot capture "... not found..." error.

import random
import os
import subprocess
import shlex
import MySQLdb
import time

DB_NAME = 'Swift_Benchmark'
DB_SERVER = 'localhost'
DB_USER = "db_client"
DB_PW = "deltacloud"

SWIFT_IP = '172.16.228.53'
SWIFT_USER = 'storage:yen'
SWIFT_PW = 'abcd'

def read_command_log(tmp_dir, log_file_name):
	fname = tmp_dir + log_file_name
	try:
		fileIN = open(fname, "r")
		fc = fileIN.read()
		fc = fc.replace("\n"," & ")
		fc = fc.replace("'","")   # strip ' char
	except:
		fc = ''

	return fc
	
def get_random_word(wordLen):
    word = ''
    for i in range(wordLen):
        word += random.choice('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz')
    return word	

def fnc_pick_a_file_size():
	file_size = (4, 32, 256, 1024, 4096, 16384, 131072, 1048576, 8388608)  # unit = KB
	#weight = (4,8,16,32,64,32,8,4,1)
	weight =    (4, 8,  16,  32,    16,    8,    1,       0,       0)
	total = sum(weight);	idx=0
	r = random.randint(0,total)  # pick a file size where weight is used for likelihood
	for x in weight:
		r = r-x
		if r<=0:
			break
		idx+=1
	
	return(file_size[idx])

def fnc_create_a_file(fileSize, tmp_dir):
	# randomlly generate a file name
	r = random.randint(1,9999999)  # pick a random number as file name
	fname = 'f' + str(r)
	cmd = 'dd if=/dev/urandom of=' + tmp_dir + fname + ' bs=1K count=' + str(fileSize)
	print(cmd)
	os.system(cmd)
	return(fname)
	
	
##--------------------------------------------------------------------------------------------
def fnc_pick_a_folder():
	# qeury the DB for the folder list
	db = MySQLdb.connect(host=DB_SERVER, user=DB_USER, passwd=DB_PW, db=DB_NAME)
	cursor = db.cursor()
	cursor.execute("SELECT * FROM Container_List")
	nrows = cursor.rowcount
	# randomly pick one folder
	r = random.randint(1,nrows) 
	# read r'th folder in the list
	row = cursor.fetchall()	
	folder = row[r-1][1]
	# close db connection
	cursor.close()
	db.commit()
	db.close()

	return(folder)

##--------------------------------------------------------------------------------------------
def fnc_pick_a_file():
	# qeury the DB for the folder list
	db = MySQLdb.connect(host=DB_SERVER, user=DB_USER, passwd=DB_PW, db=DB_NAME)
	cursor = db.cursor()
	cursor.execute("SELECT * FROM File_List")
	nrows = cursor.rowcount
	# randomly pick one folder
	r = random.randint(1,nrows) 
	# read r'th folder in the list
	row = cursor.fetchall()	
	fpath = row[r-1][2]
	fname = row[r-1][1]
	file_size = row[r-1][3]
	# close db connection
	cursor.close()
	db.commit()
	db.close()

	return(fpath, fname, file_size)

##--------------------------------------------------------------------------------------------
def fnc_read_all_files(tmp_dir):
	# qeury the DB for the folder list
	db = MySQLdb.connect(host=DB_SERVER, user=DB_USER, passwd=DB_PW, db=DB_NAME)
	cursor = db.cursor()
	cursor.execute("SELECT * FROM File_List")
	nrows = cursor.rowcount
	# read all files in the list
	row = cursor.fetchall()	
	for r in row:
		fpath = r[2];		fname = r[1];		fs = r[3]
		fnc_read_a_file(tmp_dir, fpath, fname, fs)
		
	# close db connection
	cursor.close()
	db.commit()
	db.close()

##--------------------------------------------------------------------------------------------
def fnc_write_a_file(tmp_dir):
	print("I will write a file")

	fs = fnc_pick_a_file_size()
	print("file size is ", fs)
	
	fname = fnc_create_a_file(fs, tmp_dir)
	
	## get the new file's checksum
	cmd = 'sha256sum '+ tmp_dir + fname
	p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
	p.wait()
	checksum = p.stdout.readline()
	checksum = checksum.split()[0];		checksum = checksum.decode("utf-8")
	fname2 = checksum +'.dat'
	#print(checksum)

	## rename the file to its checksum
	cmd = 'mv '+ tmp_dir + fname + ' ' + tmp_dir+fname2
	os.system(cmd)

	## pick up a folder (in the gateway) to copy to 
	## randomlly decide to use an old one or create a new one.
	if (random.random()<=0.9):  ## 90% chance to 
		dest_folder = fnc_pick_a_folder()
	else:
		dest_folder = fnc_create_a_folder()
	
	## open a transaction in db
	db = MySQLdb.connect(host=DB_SERVER, user=DB_USER, passwd=DB_PW, db=DB_NAME)
	cursor = db.cursor()
	qstr = "INSERT INTO Operation_Log SET action='write file', start_time=NOW(), file_size="+ str(fs) +", success=0, note='"  
	qstr += dest_folder + fname2 + "'"
	cursor.execute(qstr)
	t1 = time.time()

	try:
		## upload the file to Swift
		log_fname = "log_" + get_random_word(4) + ".txt"   # for saving command log
		cmd = 'cd '+ tmp_dir + ';'
		cmd += 'swift -A https://' + SWIFT_IP + ':8080/auth/v1.0 -U ' + SWIFT_USER + ' -K ' + SWIFT_PW
		cmd += ' upload ' + dest_folder + ' ' + fname2 + " 2> " + log_fname
		#~ print(cmd)
		a = os.system(cmd)
		msg = read_command_log(tmp_dir,log_fname)
		if a>0 or len(msg)>0:
			# update this transaction in DB
			et = time.time()-t1
			raise Error   # raise an exception

		# update this transaction in DB
		et = time.time()-t1
		msg = read_command_log(tmp_dir,log_fname)
		qstr = "UPDATE Operation_Log SET elapsed_time="+ str(et) +", success=1, error_msg='" +msg+ "' WHERE note='"  
		qstr += dest_folder + fname2 + "'"
		cursor.execute(qstr)
		
		# save the file's name and path to DB
		qstr = "INSERT INTO File_List Values (Now(), '"+ fname2 + "','" + dest_folder +"'," +str(fs)+")"
		cursor.execute(qstr)
	except:
		print "** ERROR ** in writting a file"
		msg = read_command_log(tmp_dir,log_fname)
		qstr = "UPDATE Operation_Log SET elapsed_time="+ str(et) +", success=0, error_msg='" +msg+ "' WHERE note='"  
		qstr += dest_folder + fname2 + "'"
		cursor.execute(qstr)
	
	# clean tmp file
	cmd = "rm " + tmp_dir + fname2
	os.system(cmd)
	
	# close db connection
	cursor.close()
	db.commit()
	db.close()

	# clean the log file
	os.system("rm " + tmp_dir+log_fname)

##--------------------------------------------------------------------------------------------
def fnc_create_a_folder():
	
	# randomly generate a sub-directory
	new_folder = 'test_' + get_random_word(5) + "_"
	print("naming a new container "+new_folder)

	## open a transaction in db
	db = MySQLdb.connect(host=DB_SERVER, user=DB_USER, passwd=DB_PW, db=DB_NAME)
	cursor = db.cursor()
	
	# save the new container to DB
	qstr = "INSERT INTO Container_List Values (Now(), '"+ new_folder +"')"
	cursor.execute(qstr)
		
	# close db connection
	cursor.close()
	db.commit()
	db.close()
	
	return(new_folder)

##--------------------------------------------------------------------------------------------
def fnc_read_a_file(tmp_dir, fpath, fname, fs):
	print("I will read a file:")
	print(fname)
	
	## open a transaction in db
	db = MySQLdb.connect(host=DB_SERVER, user=DB_USER, passwd=DB_PW, db=DB_NAME)
	cursor = db.cursor()
	qstr = "INSERT INTO Operation_Log SET action='read file', start_time=NOW(),\
	file_size="+ str(fs) +", success=0, verify_checksum=0, note='"  
	qstr += fpath + fname + "'"
	#~ print(qstr)
	cursor.execute(qstr)
	t1 = time.time()
	
	try:
		## download the file from Swift
		log_fname = "log_" + get_random_word(4) + ".txt" 
		cmd = 'cd '+ tmp_dir + ';'
		cmd += ' swift -A https://' + SWIFT_IP + ':8080/auth/v1.0 -U ' + SWIFT_USER + ' -K ' + SWIFT_PW
		cmd += ' download ' + fpath + ' ' + fname + " 2> " + log_fname
		#~ print(cmd)
		a = os.system(cmd)
		msg = read_command_log(tmp_dir,log_fname)
		if a>0 or len(msg)>0:
			et = time.time()-t1
			raise Error   # raise an exception if 

		et = time.time()-t1
		# verify file's checksum
		cmd = 'sha256sum '+ tmp_dir + fname + " 2>&1 |tee -a " + tmp_dir+log_fname
		p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
		p.wait()
		checksum = p.stdout.readline();		checksum = checksum.split()[0];
		checksum = checksum.decode("utf-8");	checksum += '.dat'
		print(checksum)
		if (checksum==fname):
			ver_cs = 1	# verify checksum = true
		else:
			ver_cs = 0
		#print(checksum)
		
		## update this transaction in DB
		msg = read_command_log(tmp_dir, log_fname)
			# filter out messages having file name
		msg = msg.replace(tmp_dir,'')
		msg = msg.replace(fname,'')
		msg = msg.replace(fname.rstrip('.dat'),'')		
		qstr = "UPDATE Operation_Log SET elapsed_time="+ str(et) +"\
		, success=1, verify_checksum="+ str(ver_cs) + ", error_msg='" + msg + "' WHERE note='"  
		qstr += fpath+fname + "' AND action='read file'"
		cursor.execute(qstr)

		# clean temporary file
		cmd = "rm " + tmp_dir + fname
		os.system(cmd)
	except:
		print "** ERROR ** in reading a file"
		## update this transaction in DB
		msg = read_command_log(tmp_dir, log_fname)
		qstr = "UPDATE Operation_Log SET elapsed_time="+ str(et) +"\
		, success=0, verify_checksum=0, error_msg='" + msg + "' WHERE note='"  
		qstr += fpath+fname + "' AND action='read file'"
		cursor.execute(qstr)

	# close db connection
	cursor.close()
	db.commit()
	db.close()
	
	# clean the log file
	os.system("rm " + tmp_dir+log_fname)
	
##--------------------------------------------------------------------------------------------
def fnc_delete_a_file(tmp_dir):
	print("I will delete a file:")

	## pick up a file from the File_List table
	res = fnc_pick_a_file()
	fpath = res[0];		fname = res[1];		fs = res[2]
	print(fname)
	
	## open a transaction in db
	db = MySQLdb.connect(host=DB_SERVER, user=DB_USER, passwd=DB_PW, db=DB_NAME)
	cursor = db.cursor()
	qstr = "INSERT INTO Operation_Log SET action='delete file', start_time=NOW(),\
	file_size="+ str(fs) +", success=0, note='"  
	qstr += fpath + fname + "'"
	cursor.execute(qstr)
	t1 = time.time()

	try:
		## delete the file from the gateway
		log_fname = "log_" + get_random_word(4) + ".txt"
		cmd = 'cd '+ tmp_dir + ';'
		cmd += 'swift -A https://' + SWIFT_IP + ':8080/auth/v1.0 -U ' + SWIFT_USER + ' -K ' + SWIFT_PW
		cmd += ' delete ' + fpath + ' ' + fname+ " 2> " + log_fname
		#~ print cmd
		a = os.system(cmd)
		msg = read_command_log(tmp_dir,log_fname)
		if a>0 or len(msg)>0:
			#~ # update this transaction in DB
			et = time.time()-t1
			raise Error   # raise an exception if 

		#~ # update this transaction in DB
		et = time.time()-t1
		msg = read_command_log(tmp_dir,log_fname)
		qstr = "UPDATE Operation_Log SET elapsed_time="+ str(et) +", success=1,"
		qstr += "error_msg='" +msg +"' WHERE note='"  
		qstr += fpath+fname + "' AND action='delete file'"
		#~ print qstr
		cursor.execute(qstr)

		## remove the file from File_List table
		qstr = "DELETE FROM File_List WHERE file_name='"+ fname +"' AND path='" + fpath + "'"
		cursor.execute(qstr)
	
	except:
		print "** ERROR ** in deleting a file"
		msg = read_command_log(tmp_dir,log_fname)
		qstr = "UPDATE Operation_Log SET elapsed_time="+ str(et) +", success=0, "
		qstr += "error_msg='" +msg +"' WHERE note='"  
		qstr += fpath+fname + "' AND action='delete file'"
		cursor.execute(qstr)
	
	# close db connection
	cursor.close()
	db.commit()
	db.close()
	# clean the log file
	os.system("rm " + tmp_dir+log_fname)

##--------------------------------------------------------------------------------------------
## clean up previous test containers
def clean_test_containers():
	cmd = 'swift -A https://' + SWIFT_IP + ':8080/auth/v1.0 -U ' + SWIFT_USER + ' -K ' + SWIFT_PW + ' list'
	p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
	p.wait()
	while True:
		container = p.stdout.readline()
		if not container:
			break
		container = container.split()[0];
		if container.find('test_')==0:		# clean up containers with name initial "test_"
			print "		deleting container: " + container
			cmd = 'swift -A https://' + SWIFT_IP + ':8080/auth/v1.0 -U ' + SWIFT_USER + ' -K ' + SWIFT_PW + ' delete ' + container
			os.system(cmd)
