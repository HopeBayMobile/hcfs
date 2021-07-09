#
# Copyright (c) 2021 HopeBayTech.
#
# This file is part of Tera.
# See https://github.com/HopeBayMobile for further info.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
from pyhcfs import parser
import pprint
import os
from itertools import islice
_HERE = os.path.dirname(__file__)

TEST_DATA_PATHS = [
        "test_data/v1/android",
        "test_data/v1/linux/"
]

pp = pprint.PrettyPrinter(indent=4)

def list_volume():
    for dir_path in TEST_DATA_PATHS:
        _TEST_FSMGR_FILENAME = str.encode(os.path.join(_HERE, dir_path, 'fsmgr'))
        ret = parser.list_volume(_TEST_FSMGR_FILENAME)
        pp.pprint(ret)
        assert ret[0][0] > 1
        assert ret[0][1] == b'hcfs_external'

def test_parse_meta():
    for dir_path in TEST_DATA_PATHS:
        _TEST_META_FILENAME = str.encode(os.path.join(_HERE, dir_path, 'meta_isdir'))
        ret = parser.parse_meta(_TEST_META_FILENAME)
        pp.pprint(ret)
        assert ret['result'] == 0
        assert ret['file_type'] == 0
        assert ret['child_number'] == 1002

def test_list_dir_inorder():
    for dir_path in TEST_DATA_PATHS:
        _TEST_META_FILENAME = str.encode(os.path.join(_HERE, dir_path, 'meta_isdir'))
        _TEST_META_FILELIST_FILENAME = str.encode(os.path.join(_HERE, dir_path, 'meta_isdir_filelist'))
        print(_TEST_META_FILELIST_FILENAME)
        f = open(_TEST_META_FILELIST_FILENAME, 'r')
        ret = { 'offset': (0, 0)}
        sum=0
        while True:
            ret = parser.list_dir_inorder(_TEST_META_FILENAME, ret['offset'], limit=33)
            files = [ x['d_name'] for x in ret['child_list'] ]
            if len(files) == 0:
                break
            #print(files)
            sum += len(files)
            for filename in files:
                assert str.encode(f.readline().strip()) == filename
        assert sum == 1002

def test_get_vol_usage():
    for dir_path in TEST_DATA_PATHS:
        _TEST_META_FILENAME = str.encode(os.path.join(_HERE, dir_path, 'FSstat'))
        ret = parser.get_vol_usage(_TEST_META_FILENAME)
        pp.pprint(ret)
        assert ret['result'] == 0
        assert ret['usage'] > 1*10**10
        assert ret['usage'] < 2*10**10

def test_list_file_blocks():
    for dir_path in TEST_DATA_PATHS:
        _TEST_META_FILENAME = str.encode(os.path.join(_HERE, dir_path, 'meta_isreg'))
        ret = parser.list_file_blocks(_TEST_META_FILENAME)
        pp.pprint(ret)
        assert ret['result'] == 0
        assert ret['ret_num'] == len(ret['block_list'])

if __name__ == '__main__':
    print("-----------------------------")
    print("- Test list_volume -")
    print("-----------------------------")
    list_volume()

    print("\n\n")
    print("-----------------------------")
    print("-      Test parse_meta      -")
    print("-----------------------------")
    test_parse_meta()

    print("\n\n")
    print("-----------------------------")
    print("-   Test list_dir_inorder   -")
    print("-----------------------------")
    test_list_dir_inorder()

    print("\n\n")
    print("-----------------------------")
    print("-     Test get_vol_usage    -")
    print("-----------------------------")
    test_get_vol_usage()

    print("\n\n")
    print("-----------------------------")
    print("-   Test list_file_blocks   -")
    print("-----------------------------")
    test_list_file_blocks()
