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

	# volume:[(str,str,str) ...] =>(host src), dest, (options)
	def __init__(self, name, image, cmd, args):
		assert os.path.exists("/var/run/docker.sock"), "Need installed docker."
		self.name = name
		self.volume = [("/etc/localtime", "/etc/localtime", "ro")]
		self.image = image
		self.cmd = cmd
		self.args = args
		self.tty = True
		self.wd = ""

	def add_volume(self, vol):
		assert isinstance(vol, tuple), "Input need 'tuple' but <" + vol + ">"
		assert len(vol) == 3, "Input need '3 element' tuple but <" + vol + ">"
		assert vol[1], "Input 'second element (destnation source)' shouldn't be empty but <" + vol + ">"
		self.volume.extend([vol])

def run(docker):
	assert isinstance(docker, Dockerable), "Input must inherit from Dockerable"
	# rm : remove when it exits(simple command), t : pseudo TTY, privileged : real root in container, 
	# e : environment variable, d : detach, run in background, w : set working directory
	cmd = ["docker run --rm --privileged"]
	cmd.extend(["-t" if docker.tty else ""])
	cmd.extend(["--name=" + docker.name])
	for (hsrc, dsrc, opt) in docker.volume:
		vol = (hsrc + ":") if hsrc else ""
		vol = vol + dsrc
		vol = vol + ((":" + opt) if opt else "")
		cmd.extend(["-v", vol])
	if docker.wd:	cmd.extend(["-w", docker.wd])
	cmd.extend([docker.image, docker.cmd, docker.args])
	try:
		subprocess.call(" ".join(cmd), shell=True)
	except Exception as e:
		terminate(docker)
		print e

def terminate(docker):
	assert isinstance(docker, Dockerable), "Input must inherit from Dockerable"
	_logger.info("[Docker] Stopping docker <" + docker.name + ">")
	subprocess.call("sudo docker rm -f " + docker.name, shell=True)

def cleanup(docker):
	assert isinstance(docker, Dockerable), "Input must inherit from Dockerable"
	_logger.info("[Docker] Cleaning up.")
	terminate(docker)
