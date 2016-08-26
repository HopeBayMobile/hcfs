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
    return ast.literal_eval(out)


def parse_meta(path):
    cmd = "python3 -c 'from pyhcfs.parser import *;import sys;sys.stdout.write(repr(parse_meta(b\"" + path + "\")))'"
    pipe = subprocess.Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    logger.debug("parse_meta" + repr((cmd, out, err)))
    return ast.literal_eval(out)


def list_dir_inorder(path, offset=(0, 0), limit=1000):
    cmd = "python3 -c 'from pyhcfs.parser import *;import sys;sys.stdout.write(repr(list_dir_inorder(b\"" + path + "\", " + str(
        offset) + ", " + str(limit) + ")))'"
    pipe = subprocess.Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    logger.debug("list_dir_inorder" + repr((cmd, out, err)))
    return ast.literal_eval(out)


def get_vol_usage(path):
    cmd = "python3 -c 'from pyhcfs.parser import *;import sys;sys.stdout.write(repr(get_vol_usage(b\"" + path + "\")))'"
    pipe = subprocess.Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    logger.debug("get_vol_usage" + repr((cmd, out, err)))
    return ast.literal_eval(out)


def list_file_blocks(path):
    cmd = "python3 -c 'from pyhcfs.parser import *;import sys;sys.stdout.write(repr(list_file_blocks(b\"" + path + "\")))'"
    pipe = subprocess.Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    logger.debug("list_file_blocks" + repr((cmd, out, err)))
    return ast.literal_eval(out)

if __name__ == '__main__':
    path = "../test_data_v2/FSstat0"
    result = get_vol_usage(path)
    print "path:" + path
    print repr(result)
    path = "../test_data_v2/FSstat2"
    result = get_vol_usage(path)
    print "path:" + path
    print repr(result)
    path = "../test_data_v2/FSstat3"
    result = get_vol_usage(path)
    print "path:" + path
    print repr(result)
    path = "../test_data_v2/FSstat147"
    result = get_vol_usage(path)
    print "path:" + path
    print repr(result)

    # result = list_file_blocks("../test_data_v2/10465/meta_10465")
    # assert isinstance(result, dict), "Not a dict result = <" + \
    #     repr(result) + ">"
    # print repr(result)
