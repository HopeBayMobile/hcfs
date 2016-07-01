from PhoneDataMgt import PhoneDataSrc
from StatDataMgt import StatDataSrc


def create_phone_src(phone_id, fsmgr, test_data_dir, inodes):
    return PhoneDataSrc(phone_id, fsmgr, test_data_dir, inodes)


def create_stat_src(stat_path):
    return StatDataSrc(stat_path)
