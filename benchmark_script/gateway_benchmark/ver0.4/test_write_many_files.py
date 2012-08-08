#!/usr/bin/python

# 05.24.2012 written by Yen

from myFunctions import *
import sys

# define parameters
tmp_dir = sys.argv[1]   # temporary directory for file creation, and read back.

# write a file first to prevent empty directory
#~ fnc_write_a_file(tmp_dir)
num_files = 5
#------------------
for i in range(num_files):
	action = random.choice('WWW')
	if action=="C":
		fnc_create_a_folder()
	elif action=="W":
		fnc_write_a_file(tmp_dir)
	elif action=="R":
		## pick up a file from the File_List table
		res = fnc_pick_a_file()
		fpath = res[0];		fname = res[1];		fs = res[2]
		fnc_read_a_file(tmp_dir, fpath, fname, fs)
	elif action=="D":
		fnc_delete_a_file()
	time.sleep(1)
