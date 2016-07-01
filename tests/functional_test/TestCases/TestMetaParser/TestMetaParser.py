import logging
import os
import time
import ast

import VarMgt
from Data import DataSrcFactory
from Harness import Harness

from metaParserAdapter import *

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(VarMgt.get_log_level())

# fuse file type definition (in fuseop.h)
# dir 0, file 1, link 2, pipe 3, socket 4

###################### API spec #######################################
# lev = list_external_volume
lev_spec = Harness({"input": [str], "output": [[(int, str)]]})
lev_nonexisted_spec = Harness({"input": [str], "output": [int]})

# TODO: Check why mtime_nsec, rdev, dev, nlink, atime_nsec, ctime_nsec and
# ino is type of int not long
output_spec = {"file_type": int, "child_number": int, "result": int}
stat_spec = {"blocks": int, "uid": int, "__unused5": int, "mtime_nsec": int, "rdev": int,
             "dev": int, "ctime": int, "__pad1": int, "blksize": int, "nlink": int,
             "mode": int, "atime_nsec": int, "mtime": int, "ctime_nsec": int, "gid": int,
             "atime": int, "ino": int, "__unused4": int, "size": int}
output_spec["stat"] = stat_spec
spec = {"input": [str], "output": [output_spec]}
# pm = parse_meta
pm_spec = Harness(spec)

# ldi = list_dir_inorder
ldi_spec = Harness({"input": [str],
                    "output": [{"result": int, "offset": (int, int), "child_list": [{"d_name": str, "inode": int, "d_type": int}]}]})
ldi_offset_spec = Harness({"input": [str, (int, int)],
                           "output": [{"result": int, "offset": (int, int), "child_list": [{"d_name": str, "inode": int, "d_type": int}]}]})
ldi_limit_spec = Harness({"input": [str, int],
                          "output": [{"result": int, "offset": (int, int), "child_list": [{"d_name": str, "inode": int, "d_type": int}]}]})
ldi_all_spec = Harness({"input": [str, (int, int), int],
                        "output": [{"result": int, "offset": (int, int), "child_list": [{"d_name": str, "inode": int, "d_type": int}]}]})
#######################################################################
############################## test_data ##################################
normal_meta_path = VarMgt.get_all_dir_meta_path()
nonexisted_meta_path = ["/no", "/such/", "/no/such/meta", "/and/directory"]
empty_meta_path = [""]

normal_offset = [(0, 0)]
invalid_offset = [(0, -1), (0, 101), (-1, 0), (1, -1),
                  (23, -1), (-1, 101), (23, 101)]
# TODO: rand number
normal_limit = [0, 1, 10, 100, 1000]
invalid_limit = [-1, -10, 1001, 1100]
#######################################################################


class TestMetaParser_00(object):

    def run(self):
        return True, "Do nothing"

# test_hcfs_parse_fsmgr_NormalFsmgr


class TestMetaParser_01(object):

    def run(self):
        fsmgr_path = VarMgt.get_test_fsmgr()
        result = list_external_volume(fsmgr_path)
        msg = lev_spec.check([fsmgr_path], [result])
        return True, msg

# test_hcfs_parse_fsmgr_NonexistFsmgr


class TestMetaParser_02(object):

    def run(self):
        nonexisted_path = ["/no", "/such/", "/no/such/fsmgr", "/and/directory"]
        msg = []
        for path in nonexisted_path:
            result = list_external_volume(path)
            sub_msg = lev_nonexisted_spec.expect([path], [-1], [result])
            msg.extend([sub_msg])
        return True, repr(msg)

# TODO: Add fsmgr empty file content
# test_hcfs_parse_fsmgr_EmptyFsmgr


class TestMetaParser_03(object):

    def run(self):
        empty_fsmgr_path = [""]
        msg = []
        for path in empty_fsmgr_path:
            result = list_external_volume(path)
            sub_msg = lev_nonexisted_spec.expect([path], [-1], [result])
            msg.extend([sub_msg])
        return True, repr(msg)

# test_hcfs_parse_meta_NormalMetapath


class TestMetaParser_04(object):

    def run(self):
        msg = []
        for stat_path, meta_path in VarMgt.get_all_data_path():
            stat_src = DataSrcFactory.create_stat_src(stat_path)
            result, expected = stat_src.get_data()
            assert result, "Cannot get stat expected data"
            result = parse_meta(meta_path)
            # Disabled atime, rdev, dev, uid, gid, mode
            # atime change when stat the expected value
            # rdev and dev hcfs don't store these value
            expected["stat"]["atime"] = result["stat"]["atime"]
            expected["stat"]["rdev"] = result["stat"]["rdev"]
            expected["stat"]["dev"] = result["stat"]["dev"]
            expected["stat"]["uid"] = result["stat"]["uid"]
            expected["stat"]["gid"] = result["stat"]["gid"]
            expected["stat"]["mode"] = result["stat"]["mode"]  # ???
            sub_msg = pm_spec.expect([meta_path], [expected], [result])
            msg.extend([sub_msg])
        return True, repr(msg)

expected_pm_err = {'file_type': 0,
                   'stat': {'size': 0, 'blocks': 0, 'uid': 0, 'mtime_nsec': 0, 'rdev': 0, 'dev': 0,
                            'mode': 0, '__pad1': 0, 'nlink': 0, 'ino': 0, 'blksize': 0, 'atime_nsec': 0,
                            'mtime': 0, 'ctime_nsec': 0, 'gid': 0, 'atime': 0, '__unused5': 0, '__unused4': 0, 'ctime': 0},
                   'child_number': 0, 'result': -1}

# test_hcfs_parse_meta_NonexistMetapath


class TestMetaParser_05(object):

    def run(self):
        nonexisted_path = ["/no", "/such/", "/no/such/meta", "/and/directory"]
        msg = []
        for path in nonexisted_path:
            result = parse_meta(path)
            sub_msg = pm_spec.expect([path], [expected_pm_err], [result])
            msg.extend([sub_msg])
        return True, repr(msg)

# TODO: Add meta empty file content
# test_hcfs_parse_meta_EmptypathMetapath


class TestMetaParser_06(object):

    def run(self):
        empty_path = [""]
        msg = []
        for path in empty_path:
            result = parse_meta(path)
            sub_msg = pm_spec.expect([path], [expected_pm_err], [result])
            msg.extend([sub_msg])
        return True, repr(msg)

###################### list_dir_inorder #######################


class LDI_TestProcedure(object):

    def __init__(self, meta_pathes, offsets, limits):
        self.meta_pathes = meta_pathes
        self.offsets = offsets
        self.limits = limits

    def run(self):
        msg = []
        # TODO: d_type???
        for meta_path in self.meta_pathes:
            result = list_dir_inorder(meta_path)
            sub_msg = ldi_spec.check([meta_path], [result])
            self.expect(result, meta_path)
            msg.extend([sub_msg])

            for offset in self.offsets:
                result = list_dir_inorder(meta_path, offset)
                sub_msg = ldi_offset_spec.check([meta_path, offset], [result])
                self.expect(result, [meta_path, offset, 1000])
                msg.extend([sub_msg])
            for limit_val in self.limits:
                result = list_dir_inorder(meta_path, limit=limit_val)
                sub_msg = ldi_limit_spec.check(
                    [meta_path, limit_val], [result])
                self.expect(result, [meta_path, (0, 0), limit_val])
                msg.extend([sub_msg])
            for offset in self.offsets:
                for limit in self.limits:
                    args = (meta_path, offset, limit)
                    result = list_dir_inorder(*args)
                    sub_msg = ldi_all_spec.check(list(args), [result])
                    self.expect(result, list(args))
                    msg.extend([sub_msg])
        return True, repr(msg)

    def expect(self, result, inputs=[]):
        pass

# test_hcfs_query_dir_list_NormalValues


class TestMetaParser_07(LDI_TestProcedure):

    def __init__(self):
        meta_pathes = normal_meta_path
        offsets = normal_offset
        # TODO: rand number
        limits = normal_limit
        LDI_TestProcedure.__init__(self, meta_pathes, offsets, limits)

    def expect(self, result, inputs=[]):
        logger.debug("inputs = " + repr(inputs) + ", result = " + repr(result))
        assert result["offset"] >= (0, 0), "Result offset must be positive num"
        assert result["result"] >= 0, "Result must be positive number"
        if len(result["child_list"]) == 0:
            return
        previous = result["child_list"][0]["d_name"]
        for inode in result["child_list"]:
            assert previous <= inode["d_name"], "Child list isn't in order "
            previous = inode["d_name"]

# test_hcfs_query_dir_list_NormalMetapathLimit_NonexistIndex


class TestMetaParser_08(object):

    def run(self):
        msg = []
        # TODO: d_type???
        for meta_path in normal_meta_path:
            for offset in invalid_offset:
                for limit in normal_limit:
                    args = (meta_path, offset, limit)
                    result = list_dir_inorder(*args)
                    sub_msg = ldi_all_spec.check(list(args), [result])
                    self.expect(result, list(args))
                    msg.extend([sub_msg])
        return True, repr(msg)

    # sorting with d_name
    def expect(self, result, inputs=[]):
        logger.debug("inputs = " + repr(inputs) + ", result = " + repr(result))
        assert result["offset"] == (0, 0), "Result offset should be (0, 0)"
        assert result["result"] < 0, "Result should be negative number, input "
        child_list = result["child_list"]
        assert len(child_list) == 0, "Result child list must be empty"

# test_hcfs_query_dir_list_NormalMetapathIndex_NonexistLimit


class TestMetaParser_09(object):

    def run(self):
        msg = []
        # TODO: d_type???
        for meta_path in normal_meta_path:
            for offset in normal_offset:
                for limit in invalid_limit:
                    args = (meta_path, offset, limit)
                    result = list_dir_inorder(*args)
                    sub_msg = ldi_all_spec.check(list(args), [result])
                    self.expect(result, list(args))
                    msg.extend([sub_msg])
        return True, repr(msg)

    # sorting with d_name
    def expect(self, result, inputs=[]):
        logger.debug("inputs = " + repr(inputs) + ", result = " + repr(result))
        assert result["offset"] == (0, 0), "Result offset should be (0, 0)"
        assert result["result"] < 0, "Result should be negative number, input "
        child_list = result["child_list"]
        assert len(child_list) == 0, "Result child list must be empty"

# test_hcfs_query_dir_list_NormalMetapath_NonexistIndexLimit


class TestMetaParser_10(object):

    def run(self):
        msg = []
        # TODO: d_type???
        for meta_path in normal_meta_path:
            for offset in invalid_offset:
                for limit in invalid_limit:
                    args = (meta_path, offset, limit)
                    result = list_dir_inorder(*args)
                    sub_msg = ldi_all_spec.check(list(args), [result])
                    self.expect(result, list(args))
                    msg.extend([sub_msg])
        return True, repr(msg)

    # sorting with d_name
    def expect(self, result, inputs=[]):
        logger.debug("inputs = " + repr(inputs) + ", result = " + repr(result))
        assert result["offset"] == (0, 0), "Result offset should be (0, 0)"
        assert result["result"] < 0, "Result should be negative number, input "
        child_list = result["child_list"]
        assert len(child_list) == 0, "Result child list must be empty"

# test_hcfs_query_dir_list_NonexistlMetapath


class TestMetaParser_11(object):

    def run(self):
        msg = []
        # TODO: d_type???
        for meta_path in nonexisted_meta_path:
            for offset in (normal_offset + invalid_offset):
                for limit in (normal_limit + invalid_limit):
                    args = (meta_path, offset, limit)
                    result = list_dir_inorder(*args)
                    sub_msg = ldi_all_spec.check(list(args), [result])
                    self.expect(result, list(args))
                    msg.extend([sub_msg])
        return True, repr(msg)

    # sorting with d_name
    def expect(self, result, inputs=[]):
        logger.debug("inputs = " + repr(inputs) + ", result = " + repr(result))
        assert result["offset"] == (0, 0), "Result offset should be (0, 0)"
        assert result["result"] < 0, "Result should be negative number, input "
        child_list = result["child_list"]
        assert len(child_list) == 0, "Result child list must be empty"

# test_hcfs_query_dir_list_EmptyMetapath


class TestMetaParser_12(object):

    def run(self):
        msg = []
        # TODO: d_type???
        for meta_path in empty_meta_path:
            for offset in (normal_offset + invalid_offset):
                for limit in (normal_limit + invalid_limit):
                    args = (meta_path, offset, limit)
                    result = list_dir_inorder(*args)
                    sub_msg = ldi_all_spec.check(list(args), [result])
                    self.expect(result, list(args))
                    msg.extend([sub_msg])
        return True, repr(msg)

    # sorting with d_name
    def expect(self, result, inputs=[]):
        logger.debug("inputs = " + repr(inputs) + ", result = " + repr(result))
        assert result["offset"] == (0, 0), "Result offset should be (0, 0)"
        assert result["result"] < 0, "Result should be negative number, input "
        child_list = result["child_list"]
        assert len(child_list) == 0, "Result child list must be empty"


class TestMetaParser_99(object):

    def run(self):
        return True, "Do nothing"


def report(msg, inputs=[], outputs=[]):
    result = msg
    if inputs:
        result = result + ";input : " + repr(inputs)
    if output:
        result = result + ";output : " + repr(outputs)
    return result

if __name__ == '__main__':
    pass
