from PhoneDataMgt import PhoneDataSrc
from LocalDataMgt import LocalMetaDataSrc

def create_phone_src(phone_id, fsmgr, test_data_dir, inodes):
	return PhoneDataSrc(phone_id, fsmgr, test_data_dir, inodes)

def create_local_src(test_data_dir):
	return LocalMetaDataSrc(test_data_dir)
