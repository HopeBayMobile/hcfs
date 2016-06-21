# See CFFI docs at https://cffi.readthedocs.org/en/latest/
import os
from cffi import FFI

ffi = FFI()

# set_source is where you specify all the include statements necessary
# for your code to work and also where you specify additional code you
# want compiled up with your extension, e.g. custom C code you've written
#
# set_source takes mostly the same arguments as distutils' Extension, see:
# https://cffi.readthedocs.org/en/latest/cdef.html#ffi-set-source-preparing-out-of-line-modules
# https://docs.python.org/3/distutils/apiref.html#distutils.core.Extension

def relpath(target):
    here = os.path.dirname( __file__ )
    if here == "":
        here = "."
    print os.path.relpath(here + "/" + target)
    return os.path.relpath(here + "/" + target)

ffi.set_source( '_pyhcfs',
    source="""
    #include "parser.h"
    """,
    include_dirs=[relpath("."), relpath("../HCFS")],
    sources=[relpath("parser.c")],
    extra_compile_args=['-D_ANDROID_ENV_'])

# declare the functions, variables, etc. from the stuff in set_source
# that you want to access from your C extension:
#https: // cffi.readthedocs.org/en/latest/cdef.html#ffi-cdef-declaring-types-and-functions
ffi.cdef(
    """
    typedef struct {
	uint64_t inode;
	char name[256];
    } PORTABLE_DIR_ENTRY;                                                                
    int32_t list_external_volume(char *meta_path , PORTABLE_DIR_ENTRY **ptr_ret_entry,   
                                 uint64_t *ret_num);                                     
    """)

if __name__ == "__main__":
        ffi.compile(verbose=True)
