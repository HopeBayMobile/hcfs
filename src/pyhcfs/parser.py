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

    cdef:
        typedef struct {
            uint64_t inode;
            char name[256];
        } PORTABLE_DIR_ENTRY;
        int32_t list_external_volume(char *meta_path,
                                     PORTABLE_DIR_ENTRY **ptr_ret_entry,
                                     uint64_t *ret_num);
    """
    ptr_ret_entry = ffi.new("PORTABLE_DIR_ENTRY **")
    ret_num = ffi.new("uint64_t *")
    ret = lib.list_external_volume(fsmgr_path, ptr_ret_entry, ret_num)
    if ret != 0:
        return ret

    response = []
    for x in range(ret_num[0]):
        entry = ptr_ret_entry[0][x]
        response += [(entry.inode, ffi.string(entry.name))]

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


def list_dir_inorder(meta_path, offset=(0, 0), limit=20):
    resp = [
        (1001, u'Android'),
        (1002, u'data'),
        (1003, u'DCIM'),
        (1004, u'Download'),
        (1005, u'LINE_Backup'),
        (1006, u'我的文件夾'),
        (1007, u'照片 2016'),
        (1008, u'企劃書 01.docx'),
        (1009, u'企劃書 02.docx'),
        (1010, u'活動投影片01.pptx'),
        (1011, u'活動投影片02.pptx'),
        (1012, u'照片 5022.jpg'),
        (1013, u'照片 5023.jpg'),
        (1014, u'照片 5024.jpg'),
        (1015, u'照片 5025.jpg'),
        (1016, u'照片 5026.jpg'),
        (1017, u'照片 5022.jpg'),
        (1018, u'照片 5023.jpg'),
        (1019, u'照片 5024.jpg'),
        (1020, u'照片 5025.jpg'),
        (1021, u'照片 5026.jpg'),
    ]

    offset = (234, 543)

    return (offset, resp)

def get_external_vol(tmp_meta):
    return list_external_vol(tmp_meta)
