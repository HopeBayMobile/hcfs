import logging

import Env

import lib

logging.basicConfig()
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)

#TODO:Just start HCFS daemon, not yet create file system and mount directory
# If you do the jobs we say previously, 
# you need to implement these jobs cleaning procedure
class TestMetaParser_00(object):
	def run(self):
		logger.info("[Global setup] Starting hcfs, swift.")
		isSuccess, log = Env.setup_Ted_env()
		if not isSuccess:	return False, log
		logger.info("[Global setup] Starting hcfs, swift.")
		return True, ""

# test_hcfs_parse_fsmgr_NormalFsmgr
# 
class TestMetaParser_01(object):
	def run(self):
		test_fsmgr_path = ["/", "/mnt", "~/", "~"]
		try:
			for data in test_fsmgr_path:
				result = lib.list_external_volume(data)
				assert isinstance(result, list), "Return type error is not a list, return = " + result
				for inode in result:
					assert isinstance(inode, tuple), "Return list element type error is not a tuple, element = " + inode
					assert isinstance(inode[0], int), "Return tuple element 'inode' type error is not a int, inode = " + inode[0]
					assert isinstance(inode[1], str), "Return tuple element 'name' type error is not a string, name = " + inode[1]
		except Exception as e:
			return False, e
		return True, ""

# test_hcfs_parse_fsmgr_NonexistFsmgr
class TestMetaParser_02(object):
	def run(self):
		return True, "dummy"

# test_hcfs_parse_fsmgr_EmptyFsmgr
class TestMetaParser_03(object):
	def run(self):
		return True, "dummy"

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
