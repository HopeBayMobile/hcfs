import nose
import sys
import os
import json
import random
import time
from ConfigParser import ConfigParser

# Add gateway sources
DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
BASEDIR = os.path.dirname(DIR)
sys.path.insert(0, os.path.join(BASEDIR, 'src'))

# Import packages to be tested
from gateway import common
from gateway import api
from gateway.common import TimeoutError
from gateway.common import timeout

class Test_timeoutDeco:
	'''
	Test if the timeout decorator works
	'''
	@timeout(2)
	def busyWait2seconds(self):
		while True:
			pass

	def setup(self):
		pass
		
	def teardown(self):
		pass

	#allow 2 seconds of delay
	@nose.tools.timed(4)
	@nose.tools.raises(TimeoutError)
	def test_delayedTimeout(self):
		'''
		test if the timeoutError is delayed
		'''
		self.busyWait2seconds()


class Test_applyStorageAccount:
	def setup(self):
		self.storage_url="172.16.228.53:8080"
		self.account = "system:pass"
		self.password = "testpass"
		
	def teardown(self):
		if os.path.exists("/root/.s3ql/authinfo2"):
			os.system("rm /root/.s3ql/authinfo2")

	def test_checkValues(self):
		'''
		test if the timeoutError is delayed
		'''
		api.apply_storage_account(storage_url=self.storage_url,
			                  account=self.account,
				          password=self.password)


		config = ConfigParser()
                with open('/root/.s3ql/authinfo2','rb') as op_fh:
                        config.readfp(op_fh)

                section = "CloudStorageGateway"
		nose.tools.ok_(config.has_section(section))

		full_url = "swift://%s"%self.storage_url
		nose.tools.eq_(full_url,
			       config.get(section, 'storage-url'))

		nose.tools.eq_(self.account,
			       config.get(section, 'backend-login'))
		
		nose.tools.eq_(self.password,
			       config.get(section, 'backend-password'))

class Test_getStorageAccount:
	def setup(self):
		self.storage_url="172.16.228.53:8080"
		self.account = "system:pass"
		self.password = "testpass"
		
	def teardown(self):
		if os.path.exists("/root/.s3ql/authinfo2"):
			os.system("rm /root/.s3ql/authinfo2")

	def test_checkValues(self):
		if os.path.exists("/root/.s3ql/authinfo2"):
			os.system("rm /root/.s3ql/authinfo2")
		api.apply_storage_account(storage_url=self.storage_url,
			                  account=self.account,
				          password=self.password)

		jsonStr = api.get_storage_account()

		outcome = json.loads(jsonStr)

		nose.tools.eq_(True,
			       outcome["result"])

		nose.tools.eq_(self.storage_url,
			       outcome["data"]["storage_url"])

		nose.tools.eq_(self.account,
			       outcome["data"]["account"])

	def test_NoAccountInfo(self):
		'''
		test if the timeoutError is delayed
		'''

		if os.path.exists("/root/.s3ql/authinfo2"):
			os.system("rm /root/.s3ql/authinfo2")

		jsonStr = api.get_storage_account()

		outcome = json.loads(jsonStr)

		nose.tools.eq_(False,
			       outcome["result"])


class Test_applyUserEncryptionKey:
	def setup(self):
		self.storage_url="172.16.228.53:8080"
		self.account = "system:pass"
		self.password = "testpass"
		
	def teardown(self):
		if os.path.exists("/root/.s3ql/authinfo2"):
			os.system("rm /root/.s3ql/authinfo2")

	def test_keyCreation(self):
		'''
		test if we can create the key
		'''
		api.apply_storage_account(storage_url=self.storage_url,
			                  account=self.account,
				          password=self.password)

		key="1233456"
		jsonStr = api.apply_user_enc_key("", key)

		outcome = json.loads(jsonStr)
		nose.tools.eq_(True,
			       outcome["result"])


		config = ConfigParser()
                with open('/root/.s3ql/authinfo2','rb') as op_fh:
                        config.readfp(op_fh)

                section = "CloudStorageGateway"

		nose.tools.eq_(key,
			       config.get(section, 'bucket-passphrase'))

	
	def test_changeKey(self):
		'''
		test if the key is successfully changed
		'''
		api.apply_storage_account(storage_url=self.storage_url,
			                  account=self.account,
				          password=self.password)

		key="123456"
		api.apply_user_enc_key(None, key)


		new_key="654321"
		jsonStr = api.apply_user_enc_key(key, new_key)
		outcome = json.loads(jsonStr)
		nose.tools.eq_(True,
			       outcome["result"])


		config = ConfigParser()
                with open('/root/.s3ql/authinfo2','rb') as op_fh:
                        config.readfp(op_fh)

                section = "CloudStorageGateway"

		nose.tools.eq_(new_key,
			       config.get(section, 'bucket-passphrase'))

	def test_wrongOldKey(self):
		'''
		test if the result is false when the old key is wrong
		'''
		api.apply_storage_account(storage_url=self.storage_url,
			                  account=self.account,
				          password=self.password)

		key="123456"
		api.apply_user_enc_key("", key)


		new_key="654321"
		jsonStr = api.apply_user_enc_key("abcdefg", new_key)
		outcome = json.loads(jsonStr)
		nose.tools.eq_(False,
			       outcome["result"])


		config = ConfigParser()
                with open('/root/.s3ql/authinfo2','rb') as op_fh:
                        config.readfp(op_fh)

                section = "CloudStorageGateway"

		nose.tools.eq_(key,
			       config.get(section, 'bucket-passphrase'))


	def test_NoAccountInfo(self):
		'''
		test if the result is false when threre is no accuontInfo
		'''
		if os.path.exists("/root/.s3ql/authinfo2"):
			os.system("rm /root/.s3ql/authinfo2")

		key="654321"
		jsonStr = api.apply_user_enc_key("abcdefg", key)
		outcome = json.loads(jsonStr)
		nose.tools.eq_(False,
			       outcome["result"])

if __name__ == "__main__":
	pass
