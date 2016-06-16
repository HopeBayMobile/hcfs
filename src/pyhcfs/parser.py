# See CFFI docs at https://cffi.readthedocs.org/en/latest/
from ._parser import ffi, lib

"""
int scalar_int_add(int a, int b);
int np_int32_add(int32_t* a, int32_t* b, int32_t* out, int size);
"""
def list_external_volume(fsmgr_path):
    """
    Add two integers.

    """
    lib.
    out = 0
    return out
    #return lib.scalar_int_add(x, y)


#def np_int32_add(x, y):
#    """
#    Add two integer NumPy arrays elementwise.
#
#    """
#    x_ptr = ffi.cast('int32_t *', x.ctypes.data)
#    y_ptr = ffi.cast('int32_t *', y.ctypes.data)
#    return 1
