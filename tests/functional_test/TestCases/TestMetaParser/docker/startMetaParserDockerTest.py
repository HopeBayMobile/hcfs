#
# Copyright (c) 2021 HopeBayTech.
#
# This file is part of Tera.
# See https://github.com/HopeBayMobile for further info.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
import os

from docker import Docker
import config

logger = config.get_logger().getChild(__name__)

if __name__ == "__main__":
    logger.info("Starting docker")
    repo = os.path.abspath(os.path.dirname(__file__))
    while not os.path.exists(repo + "/.git"):
        repo = os.path.abspath(os.path.join(repo, os.pardir))
    docker = Docker("test-meta-parser-docker", "docker:5000/docker_hcfs_test_slave", "/bin/sh",
                    "-c /hcfs/tests/functional_test/TestCases/TestMetaParser/start_test_in_docker.sh")
    docker.add_volume((repo, "/hcfs", ""))
    #docker.add_volume(("/dev/bus/usb", "/dev/bus/usb", ""))

    docker.wd = "/hcfs/tests/functional_test/TestCases/TestMetaParser"
    docker.terminate()
    docker.run()
