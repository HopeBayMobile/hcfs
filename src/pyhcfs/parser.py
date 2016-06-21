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

"""
typedef struct {
    uint64_t inode;
    char name[256];
} PORTABLE_DIR_ENTRY;                                                                
int32_t list_external_volume(char *meta_path , PORTABLE_DIR_ENTRY **ptr_ret_entry,   
                             uint64_t *ret_num);                                     
"""

def list_external_volume(fsmgr_path):
    """
    Return list of hcfs external volumes

    """
    ptr_ret_entry = ffi.new("PORTABLE_DIR_ENTRY **")
    ret_num = ffi.new("uint64_t *")
    ret = lib.list_external_volume(fsmgr_path, ptr_ret_entry, ret_num)
    if ret != 0:
        return ret
    return [convert_to_python(ptr_ret_entry[0][x]) for x in range(ret_num[0])]
