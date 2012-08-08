#!/usr/bin/python

# 04.30.2012 written by Yen
# updated from ver0.2

from myFunctions import *
import sys

# write a file first to prevent empty directory
fileIN = open("aaa.txt", "r")
fc = fileIN.read()
fc = fc.replace("\n"," & ")
fc = fc.replace("'","")   # strip ' char
print fc + " === EOF === "
