import sys, time
import socket
import pickle

sys.path.append("/DCloudSwift/util")
from SwiftCfg import SwiftCfg
from daemon import Daemon
import util

PORT=2308

class SwiftMonitor(Daemon):
	def run(self):
		logger = util.getLogger(name="SwiftMonitor")
		storageIpList = util.getStorageIpList()
		
		try:
			with open("/etc/swift/proxyList","rb") as fh:
				proxyList = pickle.load(fh)
		except IOError:
			logger.error("Failed to load proxyList")
		s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
		while True:
			time.sleep(10)
			

if __name__ == "__main__":
	daemon = SwiftMonitor('/var/run/swiftMonitor.pid')
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
