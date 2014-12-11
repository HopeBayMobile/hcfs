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