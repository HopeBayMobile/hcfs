# -*- coding: utf-8 -*-
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

# See CFFI docs at https://cffi.readthedocs.org/en/latest/
from _pyhcfs import ffi, lib
import os
import errno
import inspect
import sys

error_msg = {
        -2: "Unsupported meta version",
        }


def str_error_msg(name, ret_val):
    msg = ""
    if ret_val == -1:
        msg = name + ': ' + os.strerror(ffi.errno)
    else:
        msg = name + ": " + error_msg[ret_val]
    return msg


def __convert_struct_field(s, fields):
    for field, fieldtype in fields:
        if fieldtype.type.kind == 'primitive':
            yield (field, getattr(s, field))
        else:
            yield (field, convert_to_python(getattr(s, field)))


def convert_to_python(s):
    type = ffi.typeof(s)
    if type.kind == 'struct':
        return dict(__convert_struct_field(s, type.fields))
    elif type.kind == 'array':
        if type.item.kind == 'primitive':
            if type.item.cname == 'char':
                return ffi.string(s)
            else:
                return [s[i] for i in range(type.length)]
        else:
            return [convert_to_python(s[i]) for i in range(type.length)]
    elif type.kind == 'primitive':
        return int(s)
    else:
        return s


def list_volume(fsmgr_path):
    """
    Return list of hcfs external volumes
    """
    ptr_ret_entry = ffi.new("PORTABLE_DIR_ENTRY **")
    ret_num = ffi.new("uint64_t *")
    ret = lib.list_volume(fsmgr_path, ptr_ret_entry, ret_num)
    if ret < 0:
        print(
            'Error:',
            str_error_msg(
                inspect.stack()[0][3],
                ret),
         file=sys.stderr)
        return ret

    response = []
    for x in range(ret_num[0]):
        entry = ptr_ret_entry[0][x]
        response += [(entry.inode, entry.d_type, ffi.string(entry.d_name))]

    return response


def parse_meta(meta_path):
    """
    Get data from hcfs metafile

    cdef: #define D_ISDIR 0
        #define D_ISREG 1
        #define D_ISLNK 2
        #define D_ISFIFO 3
        #define D_ISSOCK 4
    int32_t parse_meta(char *meta_path, RET_META *meta);

    @Input  meta_path: A string contains local path of metafile
    @Output result: 0 on success, negative integer on error.
    @Output file_type: defined in fuseop.h
    @Output child_number: number of childs if it is a folder, not used if
                file_type is not D_ISDIR.
    """
    meta = ffi.new("RET_META *")
    lib.parse_meta(meta_path, meta)
    ret = convert_to_python(meta[0])
    if ret['result'] < 0:
        ret['error_msg'] = str_error_msg(inspect.stack()[0][3], ret['result'])
        print('Error:', ret['error_msg'], file=sys.stderr)
    return ret


def list_dir_inorder(meta_path="", offset=(0, 0), limit=1000):
    """
    int32_t list_dir_inorder(const char *meta_path, const int64_t page_pos,
                             const int32_t start_el, const int32_t limit,
                             int64_t *end_page_pos, int32_t *end_el_no,
                             iint32_t *num_children, PORTABLE_DIR_ENTRY *file_list);
    """
    ret = {
            'result': -1,
            'offset': (0, 0),
            'child_list': [],
            'num_child_walked': 0
            }
    if limit <= 0 or offset[0] < 0 or offset[1] < 0:
        return ret

    end_page_pos = ffi.new("int64_t *")
    end_el_no = ffi.new("int32_t *")
    num_child_walked = ffi.new("int32_t *")
    file_list = ffi.new("PORTABLE_DIR_ENTRY []", limit)

    ret_code = lib.list_dir_inorder(
        meta_path,
        offset[0],
        offset[1],
        limit,
        end_page_pos,
        end_el_no,
        num_child_walked,
        file_list)

    ret['result'] = ret_code
    ret['offset'] = (end_page_pos[0], end_el_no[0])
    if ret['result'] >= 0:
        ret['num_child_walked'] = num_child_walked[0]
        ret['child_list'] = [
            convert_to_python(file_list[i])
            for i in range(ret['num_child_walked'])]
    else:
        ret['error_msg'] = str_error_msg(inspect.stack()[0][3], ret['result'])
        print('Error:', ret['error_msg'], file=sys.stderr)
    return ret


def get_external_vol(tmp_meta):
    return list_external_vol(tmp_meta)


def get_vol_usage(meta_path=""):
    ret = {
            'result': -1,
            'usage': 0
            }

    vol_usage = ffi.new("int64_t *", 0)

    ret_code = lib.get_vol_usage(meta_path, vol_usage)
    ret['result'] = ret_code
    ret['usage'] = vol_usage[0]

    if ret['result'] < 0:
        ret['error_msg'] = str_error_msg(inspect.stack()[0][3], ret['result'])
        print('Error:', ret['error_msg'], file=sys.stderr)

    return ret


def list_file_blocks(meta_path=""):
    ret = {
            'result': -1,
            'ret_num': 0,
            'block_list': [],
            }

    block_list = ffi.new("PORTABLE_BLOCK_NAME **")
    ret_num = ffi.new("int64_t *")
    inode = ffi.new("int64_t *")
    ret_code = lib.list_file_blocks(meta_path, block_list, ret_num, inode)

    ret['result'] = ret_code

    if ret['result'] < 0:
        ret['error_msg'] = str_error_msg(inspect.stack()[0][3], ret['result'])
        print('Error:', ret['error_msg'], file=sys.stderr)

    ret['ret_num'] = ret_num[0]
    for x in range(ret_num[0]):
        ret['block_list'] += [
            'data_{0}_{1}_{2}'.format(
                inode[0],
                block_list[0][x].block_num,
                block_list[0][x].block_seq)]
    return ret
