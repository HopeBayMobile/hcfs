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
    return os.path.relpath(here + "/" + target)

ffi.set_source( '_pyhcfs',
    source="""
    #include <inttypes.h>
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

int32_t list_external_volume(char *meta_path,
                             PORTABLE_DIR_ENTRY **ptr_ret_entry,
                             uint64_t *ret_num);

struct hcfs_stat { /* 128 bytes */
	uint64_t dev;
	uint64_t ino;
	uint32_t mode;
	uint32_t __pad1;
	uint64_t nlink; /* unsigned int in android */
	uint32_t uid;
	uint32_t gid;
	uint64_t rdev;
	int64_t size;
	int64_t blksize; /* int in android */
	int64_t blocks;
	int64_t	atime; /* use aarch64 time structure */
	uint64_t atime_nsec;
	int64_t	mtime;
	uint64_t mtime_nsec;
	int64_t	ctime;
	uint64_t ctime_nsec;
	uint32_t __unused4;
	uint32_t __unused5;
};
    """)

if __name__ == "__main__":
        ffi.compile(verbose=True)
