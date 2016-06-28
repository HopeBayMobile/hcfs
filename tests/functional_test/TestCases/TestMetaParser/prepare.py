import os
import time
import shutil

from Data import DataSrcFactory
import VarMgt
import SwiftMgt

phone_id = VarMgt.get_phone_id()
fsmgr = VarMgt.get_test_fsmgr()
test_data_dir = VarMgt.get_test_data_dir()
sync_dirs = VarMgt.get_phone_sync_dirs()

swift = VarMgt.get_swift()

if __name__ == "__main__":
	data_src = DataSrcFactory.create_phone_src(phone_id, fsmgr, test_data_dir, sync_dirs)
	result, data = data_src.get_data()
	if result:
		try:
			meta_name = "meta_" + str(data["ino"])
			new_meta = os.path.join(test_data_dir, str(data["ino"]), meta_name)
			time.sleep(300)
			swift.download_file(meta_name, new_meta)
			assert os.path.isfile(new_meta), "Download <" + meta_name + "> fail"
		except Exception as e:
			clean_dir = os.path.join(test_data_dir, str(data["ino"]))
			shutil.rmtree(clean_dir)
