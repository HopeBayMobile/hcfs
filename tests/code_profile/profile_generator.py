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
import cProfile
import json
import os
CONFIG = "profile_config.js"
SRC = "../../../src"
DST = "profile_task/src"

def build_softlink():
    try:
        os.symlink(SRC, DST)
    except Exception as e:
        print e

def profile_task():
    with open(CONFIG) as fd:
         config = json.loads(fd.read())
    profile_list = config.keys()
    for module in profile_list:
        task = "globals()[\"%s\"].profile_read()"%module
        cProfile.run(task)

if __name__ == "__main__":
    build_softlink()
    from profile_task import *
    profile_task()
    os.remove(DST)