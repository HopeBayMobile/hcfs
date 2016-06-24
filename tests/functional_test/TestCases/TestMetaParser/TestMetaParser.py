import logging
import os
import time

import Env
import VarMgt

from pyhcfs.parser import *

_mnt = VarMgt.get_mnt()
_meta = VarMgt.get_meta_path()

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)


# fuse file type definition (in fuseop.h)
# dir 0, file 1, link 2, pipe 3, socket 4

#TODO: test link, pipe and socket file, ex: os.mknod
# relative to repo/mnt
test_data_dir = ["/first", "/second/test"]
test_data_file = ["/first/deeper.test", "/second/test/aa.li", "/second/test/ab.ri"]

###################### test data ######################################
normal_fsmgr_path = [_meta + "/fsmgr"]
nonexisted_fsmgr_path = ["/no", "/such/", "/no/such/fsmgr", "/and/directory"]
empty_fsmgr_path = [""]

nonexisted_meta_path = ["/no", "/such/", "/no/such/meta_file", "/and/directory"]
empty_meta_path = [""]
#######################################################################

#TODO:Just start HCFS daemon, not yet create file system and mount directory
# If you do the jobs we say previously, 
# you need to implement these jobs cleaning procedure
class TestMetaParser_00(object):
	def run(self):
		logger.info("[Global setup] Starting hcfs, swift.")
		isSuccess, log = Env.setup_Ted_env()
		if not isSuccess:	return False, log
		# repo/mnt/first/ repo/mnt/first/deeper.test
		# repo/mnt/second/test/ repo/mnt/second/test/aa.li repo/mnt/second/test/ab.ri
		for path in test_data_dir:
			os.makedirs(_mnt + path)
			assert os.path.exists(_mnt + path), "Error when produce test directory <" + _mnt + path + ">"
		for path in test_data_file:
			gen_file(path)
			assert os.path.isfile(_mnt + path), "Error when produce test file <" + _mnt + path + ">"
		#TODO: wait for sync		
		time.sleep(20)
		return True, ""

# test_hcfs_parse_fsmgr_NormalFsmgr
class TestMetaParser_01(object):
	def run(self):
		try:
			msg = ""
			for path in normal_fsmgr_path:
				result = list_external_volume(path)
				msg = msg + "input = " + repr([path]) + ", ouput = " + repr([result])
				assert isinstance(result, list), report("Return type error is not a list", [path], [result])
				for inode in result:
					assert isinstance(inode, tuple), report("Return list element type error is not a tuple, element = " + repr(inode), [path], [result])
					assert isinstance(inode[0], int), report("Return tuple first element 'inode' is not a int, inode = " + repr(inode), [path], [result])
					assert isinstance(inode[1], str), report("Return tuple second element 'name' is not a string, inode = " + repr(inode), [path], [result])
		except Exception as e:
			return False, e
		return True, msg

# test_hcfs_parse_fsmgr_NonexistFsmgr
class TestMetaParser_02(object):
	def run(self):
		try:
			msg = ""
			for path in nonexisted_fsmgr_path:
				result = list_external_volume(path)
				msg = msg + "input = " + repr([path]) + ", ouput = " + repr([result])
				assert isinstance(result, int), report("Return type error is not a int", [path], [result])
				assert result == -1, report("Non-existed path shouldn't return result", [path], [result])
		except Exception as e:
			return False, e
		return True, msg

# test_hcfs_parse_fsmgr_EmptyFsmgr
class TestMetaParser_03(object):
	def run(self):
		try:
			msg = ""
			for path in empty_fsmgr_path:
				result = list_external_volume(path)
				msg = msg + "input = " + repr([path]) + ", ouput = " + repr([result])
				assert isinstance(result, int), report("Return type error is not a int", [path], [result])
				assert result == -1, report("Empty path shouldn't return result", [path], [result])
		except Exception as e:
			return False, e
		return True, msg

#TODO: check content?
# output example
#{'file_type': 0, 
#'stat': {
#	'blocks': 0, 
#	'uid': 16888, 
#	'__unused5': 0, 
#	'mtime_nsec': 0L, 
#	'rdev': 0L, 
#	'dev': 0L, 
#	'ctime': 1466731354, 
#	'__pad1': 0, 
#	'blksize': 0, 
#	'nlink': 0L, 'mode': 4, 
#	'atime_nsec': 0L, 
#	'mtime': 1466731354, 
#	'ctime_nsec': 0L, 
#	'gid': 0, 
#	'atime': 1466731354, 
#	'ino': 2L, 
#	'__unused4': 0, 'size': 0}, 
#'child_number': 0L, 
#'result': 0}
# test_hcfs_parse_meta_NormalMetapath
class TestMetaParser_04(object):
	def run(self):
		try:
			msg = ""
			child_nums = []
			result_dir_num = 0
			result_file_num = 0
			for file_name in os.listdir(_meta):
				if os.path.isdir(os.path.join(_meta, file_name)) and file_name.startswith("sub_"):
					meta_sub = os.path.join(_meta, file_name)
					for insub_name in os.listdir(meta_sub):
						if os.path.isfile(os.path.join(meta_sub, insub_name)) and insub_name.startswith("meta"):
							meta_path = os.path.join(meta_sub, insub_name)
							out = parse_meta(meta_path)
							msg = msg + "input = " + repr([meta_path]) + ", ouput = " + repr([out])
							# result
							assert "result" in out, report("Return should include 'result'", [meta_path], [out])
							assert isinstance(out["result"], int), report("'Result' is not type of int", [meta_path], [out])
							assert out["result"] == 0, report("Existed path shouldn't return result = '-1'", [meta_path], [out])
							# child number
							assert "child_number" in out, report("Return should include 'child_number'", [meta_path], [out])
							assert isinstance(out["child_number"], long), report("'child_number' is not type of long", [meta_path], [out])
							# stat
							assert "stat" in out, report("Return should include 'stat'", [meta_path], [out])
							assert isinstance(out["stat"], dict), report("'stat' is not type of dictionary", [meta_path], [out])
							# file type
							assert "file_type" in out, report("Return should include 'file_type'", [meta_path], [out])
							assert isinstance(out["file_type"], int), report("'file_type' is not type of int", [meta_path], [out])
							assert out["file_type"] in [0,1,2,3,4], report("'file_type' should return value in [0,1,2,3,4]", [meta_path], [out])
							if out["file_type"] == 0:
								result_dir_num = result_dir_num + 1
								child_nums.extend([out["child_number"]])
							if out["file_type"] == 1:
								result_file_num = result_file_num + 1
								assert out["child_number"] == 0, report("File's 'child_number' should = '0'", [meta_path], [out])
			#TODO: need figure out associaion between test file and return result
			#assert result_dir_num == len(test_data_dir), "Result directory number<" + str(result_dir_num) + "> do not match with test one<" + str(len(test_data_dir)) + ">"
			#TODO: check child number
			#assert result_file_num == len(test_data_file), "Result file number<" + str(result_file_num) + "> do not match with test one<" + str(len(test_data_file)) + ">"
		except Exception as e:
			return False, e
		return True, msg

# test_hcfs_parse_meta_NonexistMetapath
class TestMetaParser_05(object):
	def run(self):
		try:
			msg = ""
			for path in nonexisted_meta_path:
				out = parse_meta(path)
				msg = msg + "input = " + repr([path]) + ", ouput = " + repr([out])
				assert "result" in out, report("Return should include 'result'", [path], [out])
				assert isinstance(out["result"], int), report("'Result' is not type of int", [path], [out])
				assert out["result"] == -1, report("Non-existed path should return '-1'", [path], [out])
		except Exception as e:
			return False, e
		return True, msg

# test_hcfs_parse_meta_EmptypathMetapath
class TestMetaParser_06(object):
	def run(self):
		try:
			msg = ""
			for path in empty_meta_path:
				out = parse_meta(path)
				msg = msg + "input = " + repr([path]) + ", ouput = " + repr([out])
				assert "result" in out, report("Return should include 'result'", [path], [out])
				assert isinstance(out["result"], int), report("'Result' is not type of int", [path], [out])
				assert out["result"] == -1, report("Non-existed path should return '-1'", [path], [out])
		except Exception as e:
			return False, e
		return True, msg

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
		logger.info("[Global teardown] terminate hcfs, swift, clean binary file, config files.")
		Env.cleanup()
		return True, ""

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
