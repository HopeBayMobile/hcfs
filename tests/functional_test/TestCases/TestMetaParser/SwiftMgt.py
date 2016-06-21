import logging
import subprocess
from subprocess import Popen, PIPE

logging.basicConfig()
_logger = logging.getLogger(__name__)
_logger.setLevel(logging.INFO)

class Swift(object):

	def __init__(self, user, ip, pwd, bucket):
		self._user = user
		self._ip = ip
		self._url = "https://" + ip + ":8080/auth/v1.0"
		self._pwd = pwd
		self._bucket = bucket
		self._useDocker = False
		_logger.info("[Swift] Initializing.")
		self.cleanup()
		self.create_bucket()
		self.check_bucket()

	@classmethod
	def fromDocker(cls, docker, user, pwd, bucket):
		swift = cls(user, docker.getIP(), pwd, bucket)
		swift._useDocker = True
		return swift

	def cleanup(self):
		_logger.info("[Swift] Cleaning up.")
		if not self._useDocker:
			self.rm_bucket()
		
	def rm_bucket(self):
		cmd = self._cmd_prefix()
		cmd.extend(["delete", self._bucket])
		_logger.info("[Swift] Remove bucket cmd = " + " ".join(cmd))	
		subprocess.call(" ".join(cmd), shell=True, stdout=PIPE, stderr=PIPE)

	def create_bucket(self):
		cmd = self._cmd_prefix()
		cmd.extend(["post", self._bucket])
		_logger.info("[Swift] Create bucket cmd = " + " ".join(cmd))
		subprocess.call(" ".join(cmd), shell=True, stdout=PIPE, stderr=PIPE)

	def check_bucket(self):
		cmd = self._cmd_prefix()
		cmd.extend(["list", self._bucket])
		_logger.info("[Swift] Check bucket cmd = " + " ".join(cmd))
		pipe = subprocess.Popen(" ".join(cmd), stdout=PIPE, stderr=PIPE, shell=True)
		out, err = pipe.communicate()
		assert not err, err
	
	def _cmd_prefix(self):
		return ["swift --insecure -A", self._url, "-U", self._user + ":" + self._user, "-K", self._pwd]
