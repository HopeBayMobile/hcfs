import os
import time
import shutil

from Data import DataSrcFactory
import VarMgt
import SwiftMgt

phone_id = VarMgt.get_phone_id()
fsmgr = VarMgt.get_test_fsmgr()
test_data_dir = VarMgt.get_test_data_dir()
inodes = VarMgt.get_phone_sync_inodes()

swift = VarMgt.get_swift()

if __name__ == "__main__":
	data_src = DataSrcFactory.create_phone_src(phone_id, fsmgr, test_data_dir, inodes)
	result, data = data_src.get_data()
	print result, data
	if result:
		try:
			for stat in data:
				inode = stat["stat"]["ino"]
				meta_name = "meta_" + str(inode)
				new_meta = os.path.join(test_data_dir, str(inode), meta_name)
				time.sleep(30) # poll and event driven
				swift.download_file(meta_name, new_meta)
				assert os.path.isfile(new_meta), "Download <" + meta_name + "> fail"
		finally:
			for name in os.listdir(test_data_dir):
				if name == "fsmgr":	continue
				path = os.path.join(test_data_dir, name)
				if len(os.listdir(path)) != 2:	shutil.rmtree(path)
