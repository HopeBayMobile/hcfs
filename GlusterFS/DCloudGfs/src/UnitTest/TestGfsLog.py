import nose
import sys
import os
import json

#Packages to be tested
sys.path.append('../')
from Utility.GlusterfsLog import GlusterfsLog


'''
Unit Test for GlusterfsLog.py
'''
class Test_logError:
	def setup(self):
		print "Start of Unit Test for function logError\n "
		self.gLog = GlusterfsLog("./", "report", "log")

	def teardown(self):
		print "End of Unit Test for function logError\n"
		os.system("rm -rf ./report ./log")

	def test_ContentIntegrity(self):
		'''
		logError(): Check the integrity of logError message
		'''
		msg = "test logError"
		self.gLog.logError(msg)
		fp = open("./log", "r")
		result = fp.read()
		result = result.split(":")
		result = result[-1].strip(" ")
		nose.tools.eq_(result, msg, "logError Error!!")


class Test_logEvent:
        def setup(self):
                print "Start of Unit Test for function logEvent\n "
		self.gLog = GlusterfsLog("./", "report", "log")
		self.msg = "test logError"

        def teardown(self):
                print "End of Unit Test for function logEvent\n"
		os.system("rm -rf ./report ./log")

        def test_ContentIntegrity(self):
                '''
                logEvent(): Check the integrity of logEvent message
                '''
                self.gLog.logError(self.msg)
                fp = open("./log", "r")
                result = fp.read()
                result = result.split(":")
                result = result[-1].strip(" ")
                nose.tools.eq_(result, self.msg, "logEvent Error!!")


class Test_touchFile:
        def setup(self):
                print "Start of Unit Test for function touchFile\n "
		self.gLog = GlusterfsLog("./", "report", "log")
		self.filePath = "./test1/test2"

        def teardown(self):
                print "End of Unit Test for function touchFile\n"
                os.system("rm -rf " + self.filePath)

        def test_FileExist(self):
                '''
                touchFile(): Check the existence of the file
                '''
		self.gLog.touchFile(self.filePath)
		nose.tools.ok_(os.path.isfile(self.filePath) == True, "touchFile failed!")


class Test_logProgress:
        def setup(self):
                print "Start of Unit Test for function logProgress\n "
                self.gLog = GlusterfsLog("./", "report", "log")
		self.blackList = ['ken1', 'ken2']
		self.outcome = "This is not a mount point!"

        def teardown(self):
                print "End of Unit Test for function logProgress\n"
                os.system("rm -rf ./report")

        def test_FileExist(self):
		'''
                logProgress(): Check the success of logProgress
		'''
		kwparams = {'progress':3.14159265, 'finished':False, 'code':-100, 'blackList':self.blackList, 'outcome':self.outcome}
                self.gLog.logProgress(**kwparams)
		fp = open("./report", "r+")
		result = json.loads(fp.read())
		kwparams['progress']='3.14159265'
                nose.tools.eq_(kwparams, result, "logProgress failed!")


