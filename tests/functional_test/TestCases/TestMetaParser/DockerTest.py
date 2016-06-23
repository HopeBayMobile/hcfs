import logging

import DockerMgt
from DockerMgt import Dockerable
import Var

_repo = Var.get_repo()

logging.basicConfig()
_logger1 = logging.getLogger(__name__)
_logger1.setLevel(logging.INFO)
_logger2 = _logger1.getChild("MyDocker")
_logger2.setLevel(logging.INFO)

class MyDocker(Dockerable):

	def __init__(self, name, image, cmd, args):
		Dockerable.__init__(self, name, image, cmd, args)

if __name__ == "__main__":
	_logger1.info("logger 1")
	_logger2.info("logger 2")
	docker = MyDocker("yo", "docker:5000/docker_hcfs_test_slave", "/bin/sh", "-c start_test_in_docker.sh")
	docker.add_volume((_repo, "/hcfs", ""))
	docker.wd = "/hcfs/tests/functional_test/TestCases/TestMetaParser"
	DockerMgt.terminate(docker)
	DockerMgt.run(docker)
