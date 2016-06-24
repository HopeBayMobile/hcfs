# -*- coding: utf-8 -*-
# See CFFI docs at https://cffi.readthedocs.org/en/latest/
from _pyhcfs import ffi, lib

def __convert_struct_field( s, fields ):
    for field,fieldtype in fields:
        if fieldtype.type.kind == 'primitive':
            yield (field,getattr( s, field ))
        else:
            yield (field, convert_to_python( getattr( s, field ) ))

def convert_to_python(s):
    type=ffi.typeof(s)
    if type.kind == 'struct':
        return dict(__convert_struct_field( s, type.fields ) )
    elif type.kind == 'array':
        if type.item.kind == 'primitive':
            if type.item.cname == 'char':
                return ffi.string(s)
            else:
                return [ s[i] for i in range(type.length) ]
        else:
            return [ convert_to_python(s[i]) for i in range(type.length) ]
    elif type.kind == 'primitive':
        return int(s)

def list_external_volume(fsmgr_path):
    """
    Return list of hcfs external volumes
    """
    ptr_ret_entry = ffi.new("PORTABLE_DIR_ENTRY **")
    ret_num = ffi.new("uint64_t *")
    ret = lib.list_external_volume(fsmgr_path, ptr_ret_entry, ret_num)
    if ret != 0:
        return ret

    response = []
    for x in range(ret_num[0]):
        entry = ptr_ret_entry[0][x]
        response += [(entry.inode, ffi.string(entry.d_name))]

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
    ret = lib.parse_meta(meta_path, meta)
    return convert_to_python(meta[0])


def list_dir_inorder(meta_path, offset=(0, 0), limit=1000):
    """
    int32_t list_dir_inorder(const char *meta_path, const int64_t page_pos,
                             const int32_t start_el, const int32_t limit,
                             int64_t *end_page_pos, int32_t *end_el_no,
                             PORTABLE_DIR_ENTRY *file_list);
    """
    end_page_pos = ffi.new("int64_t *")
    end_el_no = ffi.new("int32_t *")
    file_list = ffi.new("PORTABLE_DIR_ENTRY []", limit)
    ret_code = lib.list_dir_inorder(meta_path, offset[0], offset[1], limit, 
            end_page_pos, end_el_no, file_list)

    ret = {
            'result': ret_code,
            'offset': (end_page_pos[0], end_el_no[0]),
            'child_list': [ convert_to_python(file_list[i]) for i in range(limit) ]
            }

    return ret

def get_external_vol(tmp_meta):
    return list_external_vol(tmp_meta)
