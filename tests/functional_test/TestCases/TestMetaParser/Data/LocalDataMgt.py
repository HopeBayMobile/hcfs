import os
from overrides import overrides
import logging
import ast

from DataSrc import DataSrc

logging.basicConfig()
g_logger = logging.getLogger(__name__)
g_logger.setLevel(logging.INFO)

class LocalMetaDataSrc(DataSrc):
	def __init__(self, test_data_dir):
		self.test_data_dir = test_data_dir
		self.logger = g_logger.getChild("LocalMetaDataSrc")
		self.logger.setLevel(logging.INFO)

	@overrides
	def isAvailable(self):
		for dir_name in os.listdir(self.test_data_dir):
			self.logger("Dir = <" + dir_name + ">")
			inode = int(dir_name)
			inode_dir = os.path.join(self.test_data_dir, dir_name)
			self.logger("Dir = <" + dir_name + ">, check length")
			if len(os.listdir(inode_dir)) != 2:	return False
			if not (dir_name in os.listdir(inode_dir)):	return False
			if not (("meta_" + dir_name) in os.listdir(inode_dir)):	return False
		return True

	@overrides
	def fetch(self):
		result = []
		for dir_name in os.listdir(self.test_data_dir):
			inode_dir = os.path.join(self.test_data_dir, dir_name)
			self.logger("Fetch = <" + inode_dir + ">")
			for file_name in os.listdir(inode_dir):
				if not file_name.startswith("meta_"):
					stat_file = os.path.join(inode_dir, file_name)
					with open(stat_file, "rt") as fin:
						result.extend([ast.literal_eval(fin.read())])		
		return result
