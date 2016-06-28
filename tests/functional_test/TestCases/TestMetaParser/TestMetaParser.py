import logging
import os
import time

import VarMgt
from Data import DataSrcFactory
from Harness import Harness

from pyhcfs.parser import *

_mnt = VarMgt.get_mnt()
_meta = VarMgt.get_meta_path()

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

# fuse file type definition (in fuseop.h)
# dir 0, file 1, link 2, pipe 3, socket 4

###################### API spec #######################################
list_external_volume_normal_spec = Harness({"input":[str], "output":[[(long, str)]]})
list_external_volume_nonexisted_spec = Harness({"input":[str], "output":[int]})

output_spec = {"file_type":int, "child_number":long, "result":int}
stat_spec = {"blocks":int, "uid":int, "__unused5":int, "mtime_nsec":long, "rdev": long, 
             "dev":long, "ctime":int, "__pad1":int, "blksize":int, "nlink":long, 
             "mode":int, "atime_nsec":long, "mtime":int, "ctime_nsec":long, "gid":int, 
             "atime":int, "ino":long, "__unused4":int, "size":int}
output_spec["stat"] = stat_spec
spec = {"input":[str], "output":[output_spec]}
parse_meta_normal_spec = Harness(spec)
parse_meta_nonexisted_spec = Harness({"input":[str], "output":[int]})

#list_dir_inorder_spec = Harness({"input":[str], "output":[(int, str)]})
#######################################################################

class TestMetaParser_00(object):
	def run(self):
		return True, "Do nothing"

# test_hcfs_parse_fsmgr_NormalFsmgr
class TestMetaParser_01(object):
	def run(self):
		fsmgr_path = VarMgt.get_test_fsmgr()
		msg = list_external_volume_normal_spec.check([fsmgr_path], [list_external_volume(fsmgr_path)])
		return True, msg

# test_hcfs_parse_fsmgr_NonexistFsmgr
class TestMetaParser_02(object):
	def run(self):
		nonexisted_fsmgr_path = ["/no", "/such/", "/no/such/fsmgr", "/and/directory"]
		msg = []
		for path in nonexisted_fsmgr_path:
			sub_msg = list_external_volume_nonexisted_spec.expect([path], [-1], [list_external_volume(path)])
			msg.extend([sub_msg])
		return True, repr(msg)

#TODO: Add fsmgr empty file content
# test_hcfs_parse_fsmgr_EmptyFsmgr
class TestMetaParser_03(object):
	def run(self):
		empty_fsmgr_path = [""]
		msg = []
		for path in empty_fsmgr_path:
			sub_msg = list_external_volume_nonexisted_spec.expect([path], [-1], [list_external_volume(path)])
			msg.extend([sub_msg])
		return True, repr(msg)

class TestMetaParser_04(object):
	def run(self):
		msg = []
		test_data_dir = VarMgt.get_test_data_dir()	
		data_src = DataSrcFactory.create_local_src(test_data_dir)
		result, expected_data = data_src.get_data()
		assert result, "Unable to get local meta file data."
		# expected_data = [{}, {}, ...]
		print parse_meta(test_data_dir + "/../../../meta_650")
		# test all in test dir(functional_test/TestCases/TestMetaParser/test_data)
		for inode in os.listdir(test_data_dir):
			if inode == "fsmgr":	continue
			meta_path = os.path.join(test_data_dir, inode, "meta_" + inode)
			
			expected = None
			for cur in expected_data:
				if cur["stat"]["ino"] == int(inode):
					expected = cur
					break;
			assert expected, "Unable to find stat of inode = <" + inode + ">"
			result = parse_meta(meta_path)
			print result
			expected["stat"]["atime"] = result["stat"]["atime"] # access time change when stat the expected value so disabled it
			sub_msg = parse_meta_normal_spec.expect([meta_path], [expected], [result])
			msg.extend([sub_msg])
		return True, repr(msg)

# test_hcfs_parse_meta_NonexistMetapath
class TestMetaParser_05(object):
	def run(self):
		nonexisted_meta_path = ["/no", "/such/", "/no/such/meta", "/and/directory"]
		msg = []
		for path in nonexisted_meta_path:
			sub_msg = parse_meta_nonexisted_spec.expect([path], [-1], [parse_meta(path)])
			msg.extend([sub_msg])
		return True, repr(msg)

#TODO: Add meta empty file content
# test_hcfs_parse_meta_EmptypathMetapath
class TestMetaParser_06(object):
	def run(self):
		empty_meta_path = [""]
		msg = []
		for path in empty_meta_path:
			sub_msg = parse_meta_nonexisted_spec.expect([path], [-1], [parse_meta(path)])
			msg.extend([sub_msg])
		return True, repr(msg)

# test_hcfs_query_dir_list_NormalValues
class TestMetaParser_07(object):
	def run(self):
		return False, "Test not yet implemented"

# test_hcfs_query_dir_list_NormalMetapathLimit_NonexistIndex
class TestMetaParser_08(object):
	def run(self):
		return False, "Test not yet implemented"

# test_hcfs_query_dir_list_NormalMetapathIndex_NonexistLimit
class TestMetaParser_09(object):
	def run(self):
		return False, "Test not yet implemented"

# test_hcfs_query_dir_list_NormalMetapath_NonexistIndexLimit
class TestMetaParser_10(object):
	def run(self):
		return False, "Test not yet implemented"

# test_hcfs_query_dir_list_NonexistlMetapath
class TestMetaParser_11(object):
	def run(self):
		return False, "Test not yet implemented"

# test_hcfs_query_dir_list_EmptyMetapath
class TestMetaParser_12(object):
	def run(self):
		return False, "Test not yet implemented"

class TestMetaParser_99(object): 
	def run(self):
		return True, "Do nothing"

def gen_file(rel_path):
	with open(_mnt + rel_path, "wt") as fout:	
		fout.write("test data content first line\n")
		fout.write("test data content second line\n")

def report(reason, param, result):
	assert isinstance(reason, str), "Report input error, first parameter should be string"
	assert isinstance(param, list), "Report input error, second parameter should be list"
	assert isinstance(result, list), "Report input error, third parameter should be list"
	return reason + ", input = " + repr(param) + ", output = " + repr(result)

if __name__ == '__main__':
	pass
