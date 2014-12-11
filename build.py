#! /usr/bin/env python
# Build script
import sys

DEFAULT_PATH = "/tmp/" 

def build_deb():
    pass

def build_iso():
    pass

def build_firmware():
    pass

def build_pip():
    pass


if __name__ == "__main__":
   if sys.argv[1] == "deb" or sys.argv[1] == "iso" or sys.argv[1] == "firmware" or sys.argv[1] == "pip":
       print sys.argv[1] 
   else:
       print "print build.py deb|iso|firmware"
