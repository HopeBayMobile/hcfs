#!/usr/bin/python
#~ import subprocess
#~ import shlex
import os

print("Hello World!")

for i in range(10):
	cmd = 'mv abc /123'
	try:
		#p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
		#~ a = subprocess.call(shlex.split(cmd))  # a=1 if an error occurred
		a = os.system(cmd)
		print a
		if a>0:
			1/0   # raise an exception if 
	except:
		print "** ERROR ** in writting a file"
