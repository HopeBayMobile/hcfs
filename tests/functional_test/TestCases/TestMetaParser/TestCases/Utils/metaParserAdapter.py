import subprocess
from subprocess import Popen, PIPE
import ast

import config

logger = config.get_logger().getChild(__name__)


def list_external_volume(path):
    cmd = "python3 -c 'from pyhcfs.parser import *;import sys;sys.stdout.write(repr(list_external_volume(b\"" + path + "\")))'"
    pipe = subprocess.Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    logger.debug("list_external_volume" + repr((cmd, out, err)))
    assert not err, "Failed to call list_external_volume via python3:" + err
    return ast.literal_eval(out)


def parse_meta(path):
    cmd = "python3 -c 'from pyhcfs.parser import *;import sys;sys.stdout.write(repr(parse_meta(b\"" + path + "\")))'"
    pipe = subprocess.Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    logger.debug("parse_meta" + repr((cmd, out, err)))
    assert not err, "Failed to call parse_meta via python3:" + err
    return ast.literal_eval(out)


def list_dir_inorder(path, offset=(0, 0), limit=1000):
    cmd = "python3 -c 'from pyhcfs.parser import *;import sys;sys.stdout.write(repr(list_dir_inorder(b\"" + path + "\", " + str(
        offset) + ", " + str(limit) + ")))'"
    pipe = subprocess.Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    logger.debug("list_dir_inorder" + repr((cmd, out, err)))
    assert not err, "Failed to call list_dir_inorder via python3:" + err
    return ast.literal_eval(out)

if __name__ == '__main__':
    result = list_external_volume("test_data/fsmgr")
    assert isinstance(result, list), "Not a list result = <" + \
        repr(result) + ">"
    print repr(result)
    result = parse_meta("test_data/641/meta_641")
    assert isinstance(result, dict), "Not a dict result = <" + \
        repr(result) + ">"
    print repr(result)
    result = list_dir_inorder("test_data/641/meta_641", offset=(2, 3, 4))
    assert isinstance(result, dict), "Not a dict result = <" + \
        repr(result) + ">"
    print repr(result)
