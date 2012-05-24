import random
import os
import subprocess
import shlex
import MySQLdb
import time

DB_NAME = 'Gateway_Benchmark'
DB_SERVER = 'localhost'

def get_random_word(wordLen):
    word = ''
    for i in range(wordLen):
        word += random.choice('ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz')
    return word	

def fnc_pick_a_file_size():
	file_size = (4, 32, 256, 1024, 4096, 16384, 131072, 1048576, 8388608)  # unit = KB
	#file_size = (4, 32, 256, 2048, 16384)  # unit = KB
	weight = (4,8,16,32,64,32,8,4,1)
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
	db = MySQLdb.connect(host=DB_SERVER, user="db_client", passwd="deltacloud", db=DB_NAME)
	cursor = db.cursor()
	cursor.execute("SELECT * FROM Folder_List")
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
	db = MySQLdb.connect(host=DB_SERVER, user="db_client", passwd="deltacloud", db=DB_NAME)
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
	dest_folder = fnc_pick_a_folder()
	
	## open a transaction in db
	db = MySQLdb.connect(host=DB_SERVER, user="db_client", passwd="deltacloud", db=DB_NAME)
	cursor = db.cursor()
	qstr = "INSERT INTO Operation_Log SET action='write file', start_time=NOW(), file_size="+ str(fs) +", success=0, note='"  
	qstr += dest_folder + fname2 + "'"
	cursor.execute(qstr)
	t1 = time.time()

	try:
		## move the file to the gateway
		cmd = 'mv '+ tmp_dir + fname2 + ' ' + dest_folder + fname2
		a = os.system(cmd)
		if a>0:
			1/0   # raise an exception if 

		# update this transaction in DB
		et = time.time()-t1
		qstr = "UPDATE Operation_Log SET elapsed_time="+ str(et) +", success=1 WHERE note='"  
		qstr += dest_folder + fname2 + "'"
		cursor.execute(qstr)
		
		# save the file's name and path to DB
		qstr = "INSERT INTO File_List Values (Now(), '"+ fname2 +"','" + dest_folder +"'," +str(fs)+")"
		cursor.execute(qstr)
	except:
		print "** ERROR ** in writting a file"
	
	# close db connection
	cursor.close()
	db.commit()
	db.close()

##--------------------------------------------------------------------------------------------
def fnc_create_a_folder():
	# pick an existing folder first
	folder = fnc_pick_a_folder()
	
	# randomly generate a sub-directory
	subdir = get_random_word(5)
	print("creating new folder "+subdir)
	new_folder = folder + subdir + "/"

	## open a transaction in db
	db = MySQLdb.connect(host=DB_SERVER, user="db_client", passwd="deltacloud", db=DB_NAME)
	cursor = db.cursor()
	qstr = "INSERT INTO Operation_Log SET action='create folder', start_time=NOW(), success=0, note='"  
	qstr += new_folder +"'"
	cursor.execute(qstr)
	t1 = time.time()

	try:
		# create a new folder
		cmd = "mkdir -p " + new_folder
		a = os.system(cmd)
		if a>0:
			1/0   # raise an exception if 

		# update this transaction in DB
		et = time.time()-t1
		qstr = "UPDATE Operation_Log SET elapsed_time="+ str(et) +", success=1 WHERE note='"  
		qstr += new_folder + "'"
		cursor.execute(qstr)

		# save the new sub-directory to DB
		qstr = "INSERT INTO Folder_List Values (Now(), '"+ new_folder +"')"
		cursor.execute(qstr)
	except:
		print "** ERROR ** in creating a new folder"
		
	# close db connection
	cursor.close()
	db.commit()
	db.close()


##--------------------------------------------------------------------------------------------
def fnc_read_a_file(tmp_dir):
	print("I will read a file:")

	## pick up a file from the File_List table
	res = fnc_pick_a_file()
	fpath = res[0];		fname = res[1];		fs = res[2]
	print(fname)

	## clear linux cache
	cmd = "sudo echo 3 > /proc/sys/vm/drop_caches"
	os.system(cmd)	

	## open a transaction in db
	db = MySQLdb.connect(host=DB_SERVER, user="db_client", passwd="deltacloud", db=DB_NAME)
	cursor = db.cursor()
	qstr = "INSERT INTO Operation_Log SET action='read file', start_time=NOW(),\
	file_size="+ str(fs) +", success=0, verify_checksum=0, note='"  
	qstr += fpath + fname + "'"
	#~ print(qstr)
	cursor.execute(qstr)
	t1 = time.time()
	
	try:
		## copy the file from the gateway
		cmd = 'cp '+ fpath+fname + ' ' + tmp_dir
		a = os.system(cmd)
		if a>0:
			1/0   # raise an exception if 
		et = time.time()-t1

		# verify file's checksum
		cmd = 'sha256sum '+ tmp_dir + fname
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
		qstr = "UPDATE Operation_Log SET elapsed_time="+ str(et) +"\
		, success=1, verify_checksum="+ str(ver_cs) + " WHERE note='"  
		qstr += fpath+fname + "' AND action='read file'"
		cursor.execute(qstr)

		# clean temporary directory
		cmd = "rm " + tmp_dir + fname
		os.system(cmd)
	except:
		print "** ERROR ** in reading a file"
	
	# close db connection
	cursor.close()
	db.commit()
	db.close()
##--------------------------------------------------------------------------------------------
def fnc_delete_a_file():
	print("I will delete a file:")

	## pick up a file from the File_List table
	res = fnc_pick_a_file()
	fpath = res[0];		fname = res[1];		fs = res[2]
	print(fname)
	
	## open a transaction in db
	db = MySQLdb.connect(host=DB_SERVER, user="db_client", passwd="deltacloud", db=DB_NAME)
	cursor = db.cursor()
	qstr = "INSERT INTO Operation_Log SET action='delete file', start_time=NOW(),\
	file_size="+ str(fs) +", success=0, note='"  
	qstr += fpath + fname + "'"
	cursor.execute(qstr)
	t1 = time.time()

	try:
		## delete the file from the gateway
		cmd = 'rm '+ fpath+fname
		a = os.system(cmd)
		if a>0:
			1/0   # raise an exception if 

		#~ # update this transaction in DB
		et = time.time()-t1
		qstr = "UPDATE Operation_Log SET elapsed_time="+ str(et) +", success=1 WHERE note='"  
		qstr += fpath+fname + "' AND action='delete file'"
		cursor.execute(qstr)

		## remove the file from File_List table
		qstr = "DELETE FROM File_List WHERE file_name='"+ fname +"' AND path='" + fpath + "'"
		cursor.execute(qstr)
	
	except:
		print "** ERROR ** in deleting a file"
	
	# close db connection
	cursor.close()
	db.commit()
	db.close()
