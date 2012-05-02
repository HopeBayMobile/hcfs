import os, sys, time
import socket
import random
import pickle
import signal


WORKING_DIR = os.path.dirname(os.path.realpath(__file__))
BASEDIR = os.path.dirname(os.path.dirname(WORKING_DIR))
os.chdir(WORKING_DIR)
sys.path.append("%s/DCloudSwift/util"%BASEDIR)

from SwiftCfg import SwiftCfg
from daemon import Daemon
import util

PORT=2308

# deferSIGTERM decorator
def deferSIGTERM(f):
	def terminationHdlr(signum, frame):
		pass
	
	from functools import wraps
	@wraps(f)
	def wrapper(*args, **kwargs):
		oldHdlr = signal.signal(signal.SIGTERM, terminationHdlr)
		try:
			rv = f(*args, **kwargs) 
  			return rv 
		except Exception:
			raise
		finally:
			signal.signal(signal.SIGTERM, oldHdlr)

  	return wrapper #decorated function
  	
def TimeoutException(Exception):
	pass

def timeoutHdlr(signum, frame):
	raise TimeoutException()

class SwiftMonitor(Daemon):
	def __init__(self, pidfile, timeout=360):
		Daemon.__init__(self, pidfile)

		SC = SwiftCfg("%s/DCloudSwift/Swift.ini"%BASEDIR)
		self.password = SC.getKwparams()["password"]
		self.timeout = timeout

		signal.signal(signal.SIGALRM, timeoutHdlr)
		self.oldHdlr = signal.getsignal(signal.SIGTERM)

		if not util.findLine("/etc/ssh/ssh_config", "StrictHostKeyChecking no"):
			os.system("echo \"    StrictHostKeyChecking no\" >> /etc/ssh/ssh_config")

	def clearMaterials(self, peerIp):
		logger = util.getLogger(name="SwiftMonitor.clearMaterials")
		logger.info("start")

		myIp = util.getIpAddress()
		returncode =1

		try:
			cmd = "ssh root@%s rm -rf /etc/delta/%s"%(peerIp, myIp)
			logger.info(cmd)
			(status, stdout, stderr) = util.sshpass(self.password, cmd)
                	if status != 0:
                		raise util.SshpassError(stderr)

			returncode = 0

		except util.TimeoutError as err:
			logger.error("Failed to execute \"%s\" in time"%(cmd)) 
		except util.SshpassError as err:
			logger.error("Failed to execute \"%s\" for %s"%(cmd, err))
		finally:
			logger.info("end")
			return returncode

	@deferSIGTERM
	@util.tryLock()
	def copyMaterials(self):
		#TODO: delete unnecessay files
		logger = util.getLogger(name="SwiftMonitor.copymaterials")
		logger.info("start")

		returncode = 1
		try:	
			os.system("mkdir -p /etc/delta/daemon")
			os.system("rm -rf /etc/delta/daemon/*") #clear old materials
			os.system("cp -r /etc/swift /etc/delta/daemon/")
			os.system("cp -r %s/DCloudSwift /etc/delta/daemon/"%BASEDIR)
			returncode =0

		except OSError as e:
			logger.error(str(e))
		finally:
			logger.info("end")
			return returncode

	def sendMaterials(self, peerIp):
		logger = util.getLogger(name="SwiftMonitor.sendMaterials")
		logger.info("start")

		myIp = util.getIpAddress()
		returncode =1

		try:
			cmd = "ssh root@%s mkdir -p /etc/delta/%s"%(peerIp, myIp)
			logger.info(cmd)
			(status, stdout, stderr) = util.sshpass(self.password, cmd)
                	if status != 0:
                		raise util.SshpassError(stderr)

			cmd = "ssh root@%s rm -rf /etc/delta/%s/*"%(peerIp, myIp)
			logger.info(cmd)
			(status, stdout, stderr) = util.sshpass(self.password, cmd)
                	if status != 0:
                		raise util.SshpassError(stderr)

			cmd = "scp -r -o StrictHostKeyChecking=no --preserve /etc/delta/daemon/swift/ root@%s:/etc/delta/%s/"%(peerIp, myIp)
			logger.info(cmd)
			(status, stdout, stderr) = util.sshpass(self.password, cmd)
			if status !=0:
				raise util.SshpassError(stderr)

			cmd = "scp -r -o StrictHostKeyChecking=no --preserve /etc/delta/daemon/DCloudSwift/ root@%s:/etc/delta/%s/"%(peerIp, myIp)
			logger.info(cmd)
			(status, stdout, stderr) = util.sshpass(self.password, cmd)
			if status !=0:
				raise util.SshpassError(stderr)

			returncode = 0

		except util.TimeoutError as err:
			logger.error("Failed to execute \"%s\" in time"%(cmd)) 
		except util.SshpassError as err:
			logger.error("Failed to execute \"%s\" for %s"%(cmd, err))
		finally:
			logger.info("end")
			return returncode

	def doJob(self):
		logger = util.getLogger(name="SwiftMonitor.doJob")
		logger.info("start")

		try:
			myIp = util.getIpAddress()
			ipList = util.getSwiftNodeIpList()
                	if len(ipList) == 0:
               			logger.info("The swift cluster is empty!")
				return

                	peerIp = random.choice(ipList)
                	logger.info("The chosen one is %s"%peerIp)

			if self.sendMaterials(peerIp) !=0:
				logger.error("Failed to send materials to %s"%peerIp)
				return
			
			cmd = "ssh root@%s python /etc/delta/%s/DCloudSwift/CmdReceiver.py -u /etc/delta/%s/swift"%(peerIp, myIp, myIp)
			logger.info(cmd)
			(status, stdout, stderr) = util.sshpass(self.password, cmd)
			if status !=0:
				raise util.SshpassError(stderr)
			
		except util.TimeoutError as err:
			logger.error("Failed to execute \"%s\" in time"%(cmd)) 
		except util.SshpassError as err:
			logger.error("Failed to execute \"%s\" for %s"%(cmd, err))
		finally:
			if peerIp is not None:
				self.clearMaterials(peerIp)
			logger.info("end")

	def run(self):
		logger = util.getLogger(name="SwiftMonitor.run")

		while True:
			try:
				if self.copyMaterials() !=0:
					logger.error("Failed to copy materilas")
					continue
				signal.alarm(self.timeout) # triger alarm in timeout seconds
				self.doJob()
		
			except util.TryLockError as e:
				logger.error(str(e))
			except TimeoutException:
				logger.error("Timeout error")
			
			except Exception as e:
				logger.error(str(e))
				raise
			finally:
				signal.alarm(0)
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
