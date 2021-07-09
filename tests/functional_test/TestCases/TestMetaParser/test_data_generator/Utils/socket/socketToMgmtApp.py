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
from subprocess import Popen, PIPE

from .. import config
from .. import adb

logger = config.get_logger().getChild(__name__)

THIS_DIR = os.path.abspath(os.path.dirname(__file__))

BIN_NAME = "socketToMgmtApp"
BIN_LOCAL_PATH = THIS_DIR + "/socketToMgmtApp"
BIN_PHONE_PATH = "/data/socketToMgmtApp"


def setup():
    logger.info("socket setup")
    if not os.environ['ANDROID_NDK']:
        raise EnvironmentError("ANDROID_NDK environment var not found.")
    adb.check_availability()
    cleanup()

    cmd = "make"
    process = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE, cwd=THIS_DIR)
    out, err = process.communicate()
    logger.debug("setup" + repr((out, err)))
    if not os.path.isfile(BIN_LOCAL_PATH):
        raise Exception("Fail to make " + BIN_NAME)

    adb.push_as_root(BIN_LOCAL_PATH, BIN_PHONE_PATH, BIN_NAME)
    if not adb.is_file_available(BIN_PHONE_PATH):
        raise Exception("Fail to adb push bin file.")


def refresh_token():
    #assert _is_setup, "Must call API after setup."
    cmd = "." + BIN_PHONE_PATH
    return adb.exec_cmd(cmd)


def cleanup():
    logger.info("socket cleanup")
    if adb.is_file_available(BIN_PHONE_PATH):
        adb.rm_file(BIN_PHONE_PATH)
        if adb.is_file_available(BIN_PHONE_PATH):
            raise Exception("Fail to adb clean bin file.")

    cmd = "make clean"
    process = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE, cwd=THIS_DIR)
    process.communicate()
    if os.path.isfile(BIN_LOCAL_PATH):
        raise Exception("Fail to make clean " + BIN_NAME)

if __name__ == '__main__':
    setup()
    refresh_token()
    cleanup()
