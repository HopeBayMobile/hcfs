# See CFFI docs at https://cffi.readthedocs.org/en/latest/
from cffi import FFI


ffi = FFI()

# set_source is where you specify all the include statements necessary
# for your code to work and also where you specify additional code you
# want compiled up with your extension, e.g. custom C code you've written
#
# set_source takes mostly the same arguments as distutils' Extension, see:
# https://cffi.readthedocs.org/en/latest/cdef.html#ffi-set-source-preparing-out-of-line-modules
# https://docs.python.org/3/distutils/apiref.html#distutils.core.Extension
ffi.set_source(
    'pyhcfs._parser',
    """
    #include "pyhcfs.h"
    """,
    include_dirs=['src/HCFS'],
    sources=['pyhcfs.c'],
    extra_compile_args=['-D_ANDROID_ENV_'])

# declare the functions, variables, etc. from the stuff in set_source
# that you want to access from your C extension:
#https: // cffi.readthedocs.org/en/latest/cdef.html#ffi-cdef-declaring-types-and-functions
ffi.cdef(
    """
    typedef struct {
	ino_t d_ino;
	char d_name[MAX_FILENAME_LEN + 1];
	char d_type;                                            
    } DIR_ENTRY;                                                    
    int32_t list_external_volume(char *meta_path , DIR_ENTRY **ptr_ret_entry, uint64_t *ret_num);                            
    """)
