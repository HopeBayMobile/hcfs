#!/usr/bin/python
# -*- coding: utf-8 -*-

# Developed by Cloud Data Team, Cloud Technology Center, Delta Electronic Inc.
# backup gateway configuration from gateway to cloud 

import os
import json
from SwiftClient import *

class BackupToCloud():
	"""
	
	"""
	def __init__(self, fileList = None, cloudObject = None):
		self._fileList = fileList
		self._cloudObject = cloudObject
		if self._fileList is None:
			self._fileList = {
							'/etc/delta/network.info' : {
						    'user' : 'www-delta',
						    'group' : 'www-delta',
						    'chmod' : 755
						}
					   }
			print self._fileList
		if self._cloudObject is None:
			self._cloudObject = SwiftClient()

		
def main(argv = None):
	test = BackupToCloud()
if __name__ == '__main__':
	main()
		

	
