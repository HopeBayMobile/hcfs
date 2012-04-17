import sys, time

sys.path.append("/DCloudSwift/util")
from SwiftCfg import SwiftCfg
from daemon import Daemon
import util

class SwiftMonitor(Daemon):
	def run(self):
		while True:
			time.sleep(1)

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
