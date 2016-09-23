from subprocess import Popen, PIPE
import ast
import os
import shutil

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
REPORT_DIR = os.path.join(THIS_DIR, "report")
if os.path.exists(REPORT_DIR):
    shutil.rmtree(REPORT_DIR)
os.makedirs(REPORT_DIR)


class PyhcfsAdapter(object):

    @staticmethod
    def list_external_volume(path):
        return _python3_call("list_external_volume", (path,))

    @staticmethod
    def parse_meta(path):
        return _python3_call("parse_meta", (path,))

    @staticmethod
    def list_dir_inorder(path, offset=(0, 0), limit=1000):
        return _python3_call("list_dir_inorder", (path, offset, limit))

    @staticmethod
    def get_vol_usage(path):
        return _python3_call("get_vol_usage", (path,))

    @staticmethod
    def list_file_blocks(path):
        return _python3_call("list_file_blocks", (path,))

################################## private ####################################


def _python3_call(func, args):
    param = ""
    if len(args) != 0:
        for arg in args[:-1]:
            param += _arg_str(arg) + ","
        param += _arg_str(args[-1])

    imports = "from pyhcfs.parser import *;import sys;"
    func_call = "{0}({1})".format(func, param)
    write_stdout = "sys.stdout.write(repr({0}))".format(func_call)
    cmd = "python3 -c '{0}{1}'".format(imports, write_stdout)

    process = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = process.communicate()
    _log_cmd(func, cmd, out, err)
    result = ast.literal_eval(out)
    _log_fun(func, args, result)
    return result


def _arg_str(arg):
    if isinstance(arg, basestring):
        return "b\"" + arg + "\""
    else:
        return str(arg)  # danger for  str in list


def _log_fun(name, args, result):
    file_path = os.path.join(REPORT_DIR, name)
    with open(file_path, "a") as log_file:
        msg = "{0}{1}={2}\n".format(str(name), repr(args), str(result))
        log_file.write(msg)


def _log_cmd(file_name, cmd, out, err):
    file_path = os.path.join(REPORT_DIR, file_name)
    with open(file_path, "a") as log_file:
        log_file.write("-" * 120)
        log_file.write("\n")
        msg = "cmd:{0}\nout:{1}\nerr:{2}\n".format(
            str(cmd), str(out), str(err))
        log_file.write(msg)

if __name__ == '__main__':
    # path = "../test_data_v2/FSstat0"
    # result = get_vol_usage(path)
    # print "path:" + path
    # print repr(result)
    # path = "../test_data_v2/FSstat2"
    # result = get_vol_usage(path)
    # print "path:" + path
    # print repr(result)
    path = "meta_203"
    result = list_dir_inorder(path)
    print "path:" + path
    print repr(result)
    # path = "../test_data_v2/FSstat147"
    # result = get_vol_usage(path)
    # print "path:" + path
    # print repr(result)

    # result = list_file_blocks("../test_data_v2/10465/meta_10465")
    # assert isinstance(result, dict), "Not a dict result = <" + \
    #     repr(result) + ">"
    # print repr(result)
