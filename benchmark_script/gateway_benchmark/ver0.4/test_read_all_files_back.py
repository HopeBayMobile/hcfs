#!/usr/bin/python

# 05.24.2012 written by Yen

from myFunctions import *
import sys

# define parameters
tmp_dir = sys.argv[1]   # temporary directory for file creation, and read back.

# write a file first to prevent empty directory
#~ fnc_write_a_file(tmp_dir)

#------------------
fnc_read_all_files(tmp_dir)
