#!/usr/bin/python

# 04.30.2012 written by Yen
# updated from ver0.2

from myFunctions import *
import sys

print("Example usage: python <This script>.py /tmp/")

# define parameters
tmp_dir = sys.argv[1]   # temporary directory for file creation, and read back.

# write a file first to prevent empty directory
fnc_create_a_folder()
#~ fnc_write_a_file(tmp_dir)

#------------------
for i in range(0,10000):
	fnc_write_a_file(tmp_dir)
	time.sleep(1)
