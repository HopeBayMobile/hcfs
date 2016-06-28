from PhoneDataMgt import PhoneDataSrc
from LocalDataMgt import LocalMetaDataSrc

def create_phone_src(phone_id, fsmgr, test_data_dir, sync_dirs):
	return PhoneDataSrc(phone_id, fsmgr, test_data_dir, sync_dirs)

def create_local_src(test_data_dir):
	return LocalMetaDataSrc(test_data_dir)
