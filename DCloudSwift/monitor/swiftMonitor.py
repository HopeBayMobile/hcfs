import os, sys, time
import socket
import random
import pickle
import signal

sys.path.append("/DCloudSwift/util")
from SwiftCfg import SwiftCfg
from daemon import Daemon
import util

EEXIST = 17
PORT=2308
lockFile="/etc/delta/swift.lock"

class SwiftMonitor(Daemon):
	def __init__(self, pidfile, lockfile, timeout=360):
		Daemon.__init__(self, pidfile, lockfile)

		self.timeout = timeout

		signal.signal(signal.SIGALRM, SwiftMonitor.timeoutHdlr)
		self.oldHdlr = signal.getsignal(signal.SIGTERM)

		os.system("mkdir -p %s"%os.path.dirname(self.lockfile))
		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

	def TimeoutException(Exception):
		pass

	def timeoutHdlr(signum, frame):
		raise TimeoutException()

	def terminationHdlr(signum, frame):
		pass


	def disableSIGTERM(self):
		signal.signal(signal.SIGTERM, SwiftMonitor.terminationHdlr)

	def doJob(self):
		logger = util.getLogger(name="SwiftMonitor.doJob")
		logger.info("start")

		try:
			ipList = util.getSwiftNodeIpList()
                	if len(ipList) == 0:
               			logger.info("The swift cluster is empty!")
				return

                	tco = random.choice(ipList)
                	logger.info("The chosen one is %s"%tco)
			
                	vers = util.getSwiftConfVers()
			if vers < 0:
				logger.info("No valid version found!")
		finally:
			logger.info("end")

	def enableSIGTERM(self):
		signal.signal(signal.SIGTERM, self.oldHdlr)

	def run(self):
		logger = util.getLogger(name="SwiftMonitor.run")

		while True:
			fd = -1
			try:
				self.disableSIGTERM()
				signal.alarm(self.timeout) # triger alarm in timeout_time seconds
				fd = os.open(lockFile, os.O_RDWR| os.O_CREAT | os.O_EXCL, 0444)
				self.doJob()

			except OSError as e:
				if e.errno == EEXIST:
					logger.info("A confilct task is in execution")
				else:
					logger.info(str(e))
			except SwiftMonitor.TimeoutException:
				logger.error("Timeout error")
			finally:
				if fd != -1:
					os.close(fd)
					os.unlink(self.lockfile)

				signal.alarm(0)
				self.enableSIGTERM()
				time.sleep(10)


if __name__ == "__main__":
	daemon = SwiftMonitor('/var/run/swiftMonitor.pid', lockFile)
	if len(sys.argv) == 2:
		if 'start' == sys.argv[1]:
			daemon.start()
		elif 'stop' == sys.argv[1]:
			daemon.stop()
		elif 'restart' == sys.argv[1]:
			daemon.restart()
		else:
			print "Unknown command"
			sys.exit(2)
		sys.exit(0)
	else:
		print "usage: %s start|stop|restart" % sys.argv[0]
		sys.exit(2)
