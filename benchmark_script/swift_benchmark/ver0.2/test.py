#!/usr/bin/python
import os

print("Hello World!")

cmd = "cp test.py /defg"
a = os.system(cmd)
print a

#~ for i in range(10):
	#~ cmd = 'cp abc /123'
	#~ try:
		#~ #p = subprocess.Popen(cmd, shell=True, stdin=subprocess.PIPE, stdout=subprocess.PIPE)
		#~ a = subprocess.call(shlex.split(cmd))  # a=1 if an error occurred
		#~ if a==1:
			#~ 1/0   # raise an exception if 
	#~ except:
		#~ print "** ERROR ** in writting a file"
