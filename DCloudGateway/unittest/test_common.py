import nose
import sys
import os
import json
import random
import time

# Add gateway sources
DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
BASEDIR = os.path.dirname(DIR)
sys.path.insert(0, os.path.join(BASEDIR, 'src'))

# Import packages to be tested
from gateway import common
from gateway.common import TimeoutError
from gateway.common import timeout

class Test_timeoutDeco:
	'''
	Test if the timeout decorator works
	'''
	@timeout(5)
	def busyWait5seconds(self):
		while True:
			pass

	def setup(self):
		pass
		
	def teardown(self):
		pass

	#allow 5 seconds of delay
	@nose.tools.timed(10)
	@nose.tools.raises(TimeoutError)
	def test_delayedTimeout(self):
		'''
		test if the timeoutError is delayed
		'''
		self.busyWait5seconds()


if __name__ == "__main__":
	pass
