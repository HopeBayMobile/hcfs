import os


class FileType(object):
    DIR = 0
    FILE = 1
    LINK = 2
    FIFO = 3
    SOCKET = 4


class Path(object):
    THIS_DIR = os.path.abspath(os.path.dirname(__file__))
    TEST_DATA_DIR = os.path.join(THIS_DIR, "test_data_v2")
    TEST_RANDOM_DIR = os.path.join(TEST_DATA_DIR, "random")
    FSMGR_FILE = os.path.join(TEST_DATA_DIR, "fsmgr")
    FSMGR_STAT = os.path.join(TEST_DATA_DIR, "fsmgr_stat")
