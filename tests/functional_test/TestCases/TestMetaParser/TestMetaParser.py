import logging
import os

import Env
import Var

import lib

_mnt = Var.get_mnt()
_meta = Var.get_meta_path()

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)


# fuse file type definition (in fuseop.h)
# dir 0, file 1, link 2, pipe 3, socket 4

#TODO: test link, pipe and socket file
# relative to repo/mnt
test_data_dir = ["/first", "/second/test"]
test_data_file = ["/first/deeper.test", "/second/test/aa.li", "/second/test/ab.ri"]

normal_fsmgr_path = ["/fsmgr", "/"]
nonexisted_fsmgr_path = ["/no", "/such/", "/no/such/fsmgr", "/and/directory"]
empty_fsmgr_path = ["", None]

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
		return True, ""

# test_hcfs_parse_fsmgr_NormalFsmgr
class TestMetaParser_01(object):
	def run(self):
		try:
			for path in normal_fsmgr_path:
				result = lib.list_external_volume(_mnt + path)
				assert isinstance(result, list), report("Return type error is not a list", [path], [result])
				for inode in result:
					assert isinstance(inode, tuple), report("Return list element type error is not a tuple, element = " + repr(inode), [path], [result])
					assert isinstance(inode[0], int), report("Return tuple first element 'inode' is not a int, inode = <" + inode[0] + ">", [path], [result])
					assert isinstance(inode[1], str), report("Return tuple second element 'name' is not a string, inode = <" + inode[1] + ">", [path], [result])
		except Exception as e:
			return False, e
		return True, ""

# test_hcfs_parse_fsmgr_NonexistFsmgr
class TestMetaParser_02(object):
	def run(self):
		try:
			for path in nonexisted_fsmgr_path:
				result = lib.list_external_volume(path)
				assert isinstance(result, list), report("Return type error is not a list", [path], [result])
				assert not result, report("Non-existed path shouldn't return result", [path], [result])
		except Exception as e:
			return False, e
		return True, ""

# test_hcfs_parse_fsmgr_EmptyFsmgr
class TestMetaParser_03(object):
	def run(self):
		try:
			for path in empty_fsmgr_path:
				result = lib.list_external_volume(path)
				assert isinstance(result, list), report("Return type error is not a list", [path], [result])
				assert not result, report("Empty path shouldn't return result", [path], [result])
		except Exception as e:
			return False, e
		return True, ""

# test_hcfs_parse_meta_NormalMetapath
class TestMetaParser_04(object):
	def run(self):
		return True, "dummy"

# test_hcfs_parse_meta_NonexistMetapath
class TestMetaParser_05(object):
	def run(self):
		return True, "dummy"

# test_hcfs_parse_meta_EmptypathMetapath
class TestMetaParser_06(object):
	def run(self):
		return True, "dummy"

# test_hcfs_query_dir_list_NormalValues
class TestMetaParser_07(object):
	def run(self):
		return True, "dummy"

# test_hcfs_query_dir_list_NormalMetapathLimit_NonexistIndex
class TestMetaParser_08(object):
	def run(self):
		return True, "dummy"

# test_hcfs_query_dir_list_NormalMetapathIndex_NonexistLimit
class TestMetaParser_09(object):
	def run(self):
		return True, "dummy"

# test_hcfs_query_dir_list_NormalMetapath_NonexistIndexLimit
class TestMetaParser_10(object):
	def run(self):
		return True, "dummy"

# test_hcfs_query_dir_list_NonexistlMetapath
class TestMetaParser_11(object):
	def run(self):
		return True, "dummy"

# test_hcfs_query_dir_list_EmptyMetapath
class TestMetaParser_12(object):
	def run(self):
		return True, "dummy"

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
