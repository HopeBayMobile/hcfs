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

# DisableSIGTERM decorator
def disableSIGTERM():
	def decorator(f):
		def terminationHdlr(signum, frame):
			pass

		def f_disable(*args, **kwargs):
			oldHdlr = signal.signal(signal.SIGTERM, terminationHdlr)
			try:
				rv = f(*args, **kwargs) 
  				return rv 
			except Exception:
				raise
			finally:
				signal.signal(signal.SIGTERM, oldHdlr)

  		return f_disable #decorated function
  	
  	return decorator  #true decorator

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


	@disableSIGTERM
	def copyMaterials(self):
		#TODO: delete unnecessay files

		fd = -1
		returncode = 1
		try:
			fd = os.open(lockFile, os.O_RDWR| os.O_CREAT | os.O_EXCL, 0444)
			os.system("mkdir -p /etc/delta/daemon")
			os.system("rm -rf /etc/delta/daemon/*") #clear old materials
			os.system("cp -r /etc/swift /etc/delta/daemon/")
			os.system("cp -r /DCloudSwift /etc/delta/daemon/")
			returncode =0

		except OSError as e:
			if e.errno == EEXIST:
				logger.info("A confilct task is in execution")
			else:
				logger.error(str(e))
		finally:
			if fd != -1:
				os.close(fd)
				os.unlink(self.lockfile)

			return returncode

	def sendMaterials(self, peerIp):
		logger = util.getLogger(name="SwiftMonitor.sendMaterials")
		logger.info("start")

		myIp = util.getIpAddress()
		returncode =1

		try:
			cmd = "ssh root@%s mkdir -p /etc/delta/%s"%(peerIp, myIp)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=360)
                	if status != 0:
                		raise SshpassError(stderr)

			cmd = "ssh root@%s rm -rf /etc/delta/%s/*"%(peerIp, myIp)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=360)
                	if status != 0:
                		raise SshpassError(stderr)

			logger.info("scp -r -o StrictHostKeyChecking=no --preserve /etc/delta/swift/ root@%s:/etc/delta/%s/"%(peerIp, myIp))
			cmd = "scp -r -o StrictHostKeyChecking=no --preserve /etc/delta/swift/ root@%s:/etc/delta/%s/"%(peerIp, myIp)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=360)
			if status !=0:
				raise SshpassError(stderr)

			logger.info("scp -r -o StrictHostKeyChecking=no --preserve /DCloudSwift/ root@%s:/etc/delta/%s/"%(peerIp, myIp))
			cmd = "scp -r -o StrictHostKeyChecking=no --preserve /DCloudSwift/ root@%s:/etc/delta/%s/"%(peerIp, myIp)
			(status, stdout, stderr) = sshpass(password, cmd, timeout=360)
			if status !=0:
				raise SshpassError(stderr)

		except TimeoutError as err:
			logger.error("Failed to execute \"%s\" in time"%(cmd)) 
		except SshpassError as err:
			logger.error("Failed to execute \"%s\" for %s"%(cmd, err))
		finally:
			logger.info("end")
			return returncode

	def doJob(self):
		logger = util.getLogger(name="SwiftMonitor.doJob")
		logger.info("start")

		try:
			ipList = util.getSwiftNodeIpList()
                	if len(ipList) == 0:
               			logger.info("The swift cluster is empty!")
				return

                	peerIp = random.choice(ipList)
                	logger.info("The chosen one is %s"%peerIp)

			if self.sendMaterials(peerIp) !=0:
				logger.error("Failed to send materials to %s"%peerIp)
				return
			
		finally:
			logger.info("end")

	def run(self):
		logger = util.getLogger(name="SwiftMonitor.run")

		while True:
			try:
				signal.alarm(self.timeout) # triger alarm in timeout_time seconds
				if self.copyMaterials() !=0:
					logger.error("Failed to copy materilas")
					continue
				self.doJob()

			except SwiftMonitor.TimeoutException:
				logger.error("Timeout error")
			
			except Exception as e:
				logger.error(str(e))
				raise
			finally:
				signal.alarm(0)
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
