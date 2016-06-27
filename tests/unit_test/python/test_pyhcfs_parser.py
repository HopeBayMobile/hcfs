from pyhcfs import parser
import pprint
import os
from itertools import islice
_HERE = os.path.dirname(__file__)


pp = pprint.PrettyPrinter(indent=4)

def test_list_external_volume():
    _TEST_FSMGR_FILENAME = str.encode(os.path.join(_HERE, 'test_nexus_5x', 'fsmgr'))
    ret = parser.list_external_volume(_TEST_FSMGR_FILENAME)
    pp.pprint(ret)
    assert ret == [(187, b'hcfs_external')]

def test_parse_meta():
    _TEST_META_FILENAME = str.encode(os.path.join(_HERE, 'test_nexus_5x', 'meta'))
    ret = parser.parse_meta(_TEST_META_FILENAME)
    pp.pprint(ret)
    assert ret == {   
            'child_number': 3001,
            'file_type': 0,
            'result': 0,
            'stat': {   '__pad1': 0,
                '__unused4': 0,
                '__unused5': 0,
                'atime': 1466493536,
                'atime_nsec': 0,
                'blksize': 1048576,
                'blocks': 0,
                'ctime': 1466493534,
                'ctime_nsec': 0,
                'dev': 0,
                'gid': 2000,
                'ino': 423,
                'mode': 16895,
                'mtime': 1466493534,
                'mtime_nsec': 0,
                'nlink': 2,
                'rdev': 0,
                'size': 0,
                'uid': 2000}
            }


def test_list_dir_inorder():
    _TEST_META_FILENAME = str.encode(os.path.join(_HERE, 'test_nexus_5x', 'meta'))
    _TEST_META_FILELIST_FILENAME = str.encode(os.path.join(_HERE, 'test_nexus_5x', 'meta_filelist'))
    f = open(_TEST_META_FILELIST_FILENAME, 'r')
    offset = (0, 0)
    ret = parser.list_dir_inorder(_TEST_META_FILENAME, offset, limit=50)
    pp.pprint(ret)
    ret = parser.list_dir_inorder(_TEST_META_FILENAME, ret['offset'], limit=50)
    pp.pprint(ret)
    ret = parser.list_dir_inorder(_TEST_META_FILENAME, ret['offset'], limit=50)
    pp.pprint(ret)
