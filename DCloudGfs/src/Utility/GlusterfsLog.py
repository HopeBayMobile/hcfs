'''
Created on 2011/11/14

@authors: CW, Ken, and Rice

Modified by CW on 2011/11/22
'''

import sys
import os
import posixfile
import json
from decimal import *
from datetime import datetime
from ConfigParser import ConfigParser


class GlusterfsLog:
	def __init__(self, reportDir = "", report = "", logFile = ""):
		self.__reportDir = reportDir
		self.__report = report
		self.__logFile = logFile

	def touchFile(self, filePath):
                '''
                create filePath if filePath does not exist
                @filePath: full name of a file including parent directories     
                '''
                #Note: are mkdir, touch, chmod thread-safe?
                #Todo: chech if the command is executed succesfully
                if not os.path.isfile(filePath):
                        dirname = os.path.dirname(filePath)
                        os.system("sudo mkdir -p %s" % dirname)
                        os.system("sudo touch %s" % filePath)
                        #Todo: disable other users from modifying the file
                        os.system("sudo chmod 666 %s" % filePath)

        def logError(self, errMsg):
                '''
                write error message to the log file
                @errMsg: error message
                '''
                fp = posixfile.open(self.__logFile, 'a+')
                fp.lock('w|')
                header = str(datetime.now()) + " [Error]: "
                fp.write(header + errMsg)
                fp.lock('u')
                fp.close()

        def logEvent(self, msg):
                fp = posixfile.open(self.__logFile, 'a+')
                fp.lock('w|')
                header = str(datetime.now()) + " [Event]: "
                fp.write(header + msg)
                fp.lock('u')
                fp.close()

        def initProgress(self):
                self.logProgress(0)

        def logProgress(self, progress, finished = False, code = 0, blackList = [], outcome = ""):
                '''
                convert current progress into json string and write it into report 
                @progress: percentage of completion
                @finished: is the task finished?
                @code: 0 iff all hosts successully execute 
                @blackList: list of hosts failed to execute the cmd
		@outcome: outcome of the function invoking logProgree()
                '''
                result = {'progress':str(progress), 'finished':finished, 'code':code, 'blackList':blackList, 'outcome':outcome}
                fp = posixfile.open(self.__report, 'w')
                fp.lock('w|')
                fp.write("%s" % json.dumps(result, sort_keys = True, indent = 4))
                fp.lock('u')
                fp.close()

        def readReport(self, taskId):
                filePath = self.__reportDir + '/' + taskId + '/report'
                self.touchFile(filePath)
                fp = posixfile.open(filePath, 'a+')
                fp.lock('w|')
                jsonStr = fp.read()
                fp.lock('u')

                try:
                        report = json.loads(jsonStr)
                except ValueError:
                        report = None

                return report

        def readProgress(self, taskId):
                report = self.readReport(taskId)
                if report is None:
                        return 0
                return Decimal(report['progress'])

	def initReport(self):
                '''
                Clear the directroy containing the report and create an new empty report
                '''
                dirname = os.path.dirname(self.__report)
                os.system("sudo rm -rf %s" % dirname)
                os.system("sudo mkdir -p %s" % dirname)
                os.system("sudo touch %s" % self.__report)
                #Todo: disable other users from modifying the file
                os.system("sudo chmod 666 %s" % self.__report)


