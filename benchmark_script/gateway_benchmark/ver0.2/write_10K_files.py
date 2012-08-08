#!/usr/bin/python

# 04.30.2012 written by Yen
# updated from ver0.2

from myFunctions import *
import sys

# define parameters
tmp_dir = sys.argv[1]   # temporary directory for file creation, and read back.

# write a file first to prevent empty directory
fnc_write_a_file(tmp_dir)

#------------------
for x in range(10000):
	action = random.choice('WWWWWWWW')
	if action=="C":
		fnc_create_a_folder()
	elif action=="W":
		fnc_write_a_file(tmp_dir)
	elif action=="R":
		fnc_read_a_file(tmp_dir)
	elif action=="D":
		fnc_delete_a_file()
	time.sleep(1)
