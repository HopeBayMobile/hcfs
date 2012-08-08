#!/usr/bin/python

# 04.30.2012 written by Yen
# updated from ver0.2

from myFunctions import *
import sys

print("Example usage: python <This script>.py /tmp/")

# define parameters
tmp_dir = sys.argv[1]   # temporary directory for file creation, and read back.

fnc_read_all_files(tmp_dir)
#------------------
