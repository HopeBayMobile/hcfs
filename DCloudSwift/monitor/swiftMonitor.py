import os, sys, time
import socket
import random
import pickle

sys.path.append("/DCloudSwift/util")
from SwiftCfg import SwiftCfg
from daemon import Daemon
import util

PORT=2308
lockFile="/etc/delta/swift.lock"

class SwiftMonitor(Daemon):
	def run(self):
		logger = util.getLogger(name="SwiftMonitor")
		
		while True:
			fd = -1
			try:
				os.system("mkdir -p %s"%os.path.dirname(self.lockFile))
				fd = os.open(lockFile, os.O_RDWR| os.O_CREAT | os.O_EXCL, 0444)

				if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
					os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

				ipList = util.getSwiftNodeIpList()
                        	if len(ipList) == 0:
                                	logger.info("The swift cluster is empty!")
                                	time.sleep(10)
                                	continue

                        	tco = random.choice(ipList)
                        	logger.info("The chosen one is %s"%tco)

                        	vers = util.getSwiftConfVers()

                        	time.sleep(10)


			finally:
				if fd != -1:
					os.close(fd)
					os.unlink(self.lockFile)
			

if __name__ == "__main__":
	daemon = SwiftMonitor('/var/run/swiftMonitor.pid', lockFile=lockFile)
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
