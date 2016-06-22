import logging

import DockerMgt
from DockerMgt import Dockerable

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
	docker = MyDocker("yo", "test_img:1.0", "python", "pi_tester.py -d debug -s TestSuites/TestMetaParser.csv")
	docker.add_volume(("/home/test/hcfs", "/home/yo/hcfs", ""))
	docker.wd = "/home/yo/hcfs/tests/functional_test/"
	DockerMgt.run(docker)
