#!/usr/bin/python

# 04.30.2012 written by Yen
# updated from ver0.2

from myFunctions import *
import sys

print("Example usage: python benchmark_v?.py /tmp/")

# define parameters
tmp_dir = sys.argv[1]   # temporary directory for file creation, and read back.

# write a file first to prevent empty directory
fnc_create_a_folder()
fnc_write_a_file(tmp_dir)

#------------------
#~ while True:
for i in range(200):
	action = random.choice('WWWWWWWWWWWWWWRRRRRRRRRDD')
	if action=="W":
		fnc_write_a_file(tmp_dir)
	elif action=="R":
		fnc_read_a_file(tmp_dir)
	elif action=="D":
		fnc_delete_a_file()
	time.sleep(1)
