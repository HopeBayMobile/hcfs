# Any object which wants to run on docker, must inherit from Dockerable
import os
import logging
import subprocess
from subprocess import Popen, PIPE

logging.basicConfig()
_logger = logging.getLogger(__name__)
_logger.setLevel(logging.INFO)

# TODO: not yet implemented
class Dockerable(object):

	# volume:[(str,str,str), (str,str), (str) ...] =>(host src), dest, (options)
	def __init__(self, name:str, image:str):
		assert os.path.isfile("/var/run/docker.sock"), "Need installed docker."
		self.name = name
		self.volume = [("/etc/localtime", "/etc/localtime", "ro")]
		self.image = image
		self.tty = True
		self.detach = True

def run(docker:Dockerable):
	subprocess.call(" ".join(cmd), shell=True)

def terminate(docker:Dockerable):
	_logger.info("[Docker] Stopping docker <" + docker._name + ">")
	subprocess.call("sudo docker rm -f" + docker._name, shell=True)

def cleanup(self):
	_logger.info("[Docker] Cleaning up.")
	self.terminate()
