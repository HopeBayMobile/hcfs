import subprocess
from subprocess import Popen, PIPE
import os
import time


def isAvailable(serialno=""):
    cmd = "adb get-serialno"
    process = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = process.communicate()
    if "no devices" in out:
        return False
    if serialno and serialno not in out:
        return False
    return True


def isAvailable_and_sys_ready_until(timeout, serialno=""):
    while not isAvailable(serialno) and timeout >= 0:
        time.sleep(10)
        timeout -= 10
    while not is_file_available("/sdcard/") and timeout >= 0:
        time.sleep(10)
        timeout -= 10
    return timeout >= 0


def pull(src, dest, serialno=""):
    assert is_file_available(src, serialno), "No such file or dir"
    cmd = __get_cmd_prefix(serialno) + " pull " + src + " " + dest
    process = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    return process.communicate()


def push_as_root(src, dest, name, serialno=""):
    push(src, "/data/local/tmp", serialno)
    cmd = "mv /data/local/tmp/" + name + " " + dest
    return exec_cmd(cmd, serialno)


def push(src, dest, serialno=""):
    cmd = __get_cmd_prefix(serialno) + " push " + src + " " + dest
    process = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    return process.communicate()


def get_logcat(path, tag="HopeBay", serialno=""):
    cmd = __get_cmd_prefix(serialno) + " logcat -d -s " + tag
    cmd = cmd + " | tee " + path
    subprocess.call(cmd, shell=True, stdout=PIPE, stderr=PIPE)


def exec_cmd(cmd, serialno=""):
    prefix = __get_cmd_prefix(serialno) + " shell su 0 "
    process = Popen(prefix + cmd, shell=True, stdout=PIPE, stderr=PIPE)
    return process.communicate()


def get_file(name, src, dest, serialno=""):
    cmd = "cp " + src + " /sdcard/Download/"
    exec_cmd(cmd, serialno)
    out, err = pull("/sdcard/Download/" + name, dest)
    rm_file("/sdcard/Download/" + name)
    return out, err


def get_hcfs_log(path):
    return get_file("hcfs_android_log", "/data/hcfs_android_log", path)


def get_file_size(path, serialno=""):
    cmd = "stat -c%s" + path
    return exec_cmd(cmd, serialno)


def reboot(timeout=120, serialno=""):
    cmd = __get_cmd_prefix(serialno) + " reboot"
    pipe = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)
    out, err = pipe.communicate()
    assert not out and not err, "Should not output anything."
    time.sleep(60)
    if not isAvailable_and_sys_ready_until(timeout, serialno):
        raise TimeoutError("Timeout when wait device bootup")


def reboot_async(serialno=""):
    cmd = __get_cmd_prefix(serialno) + " reboot"
    process = Popen(cmd, shell=True, stdout=PIPE, stderr=PIPE)


def gen_dir(path, serialno=""):
    cmd = "mkdir " + path
    return exec_cmd(cmd, serialno)


def rm_file(path, serialno=""):
    cmd = "rm -rf " + path
    return exec_cmd(cmd, serialno)


def enable_wifi(serialno=""):
    return exec_cmd("svc wifi enable", serialno)


def disable_wifi(serialno=""):
    return exec_cmd("svc wifi disable", serialno)


def start_app(pkg, activity, serialno=""):
    cmd = "am start  " + pkg + "/." + activity
    return exec_cmd(cmd, serialno)


def is_file_available(path, serialno=""):
    cmd = "ls " + path + " | grep 'No such file or directory'"
    out, err = exec_cmd(cmd, serialno)
    return True if not out else False


def __get_cmd_prefix(serialno=""):
    return "adb {0}".format("-s" + serialno if serialno else "")
