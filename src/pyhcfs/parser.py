
# See CFFI docs at https://cffi.readthedocs.org/en/latest/
from ._parser import ffi, lib


def scalar_int_add(x, y):
    """
    Add two integers.

    """
    return lib.scalar_int_add(x, y)


def np_int32_add(x, y):
    """
    Add two integer NumPy arrays elementwise.

    """
    x_ptr = ffi.cast('int32_t *', x.ctypes.data)
    y_ptr = ffi.cast('int32_t *', y.ctypes.data)
    return 1
