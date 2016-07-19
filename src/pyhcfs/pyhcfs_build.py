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
    #include "parser.h"
    #include "hcfs_stat.h"
    """,
    include_dirs=[relpath("."), relpath("../HCFS")],
    sources=[relpath("parser.c")],
    extra_compile_args=['-D_ANDROID_ENV_'])

# declare the functions, variables, etc. from the stuff in set_source
# that you want to access from your C extension:
#https: // cffi.readthedocs.org/en/latest/cdef.html#ffi-cdef-declaring-types-and-functions
ffi.cdef(
    """
#define LIST_DIR_LIMIT 1000

typedef struct { /* 128 bytes */
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
} HCFS_STAT;

typedef struct {
	uint64_t inode;
	char d_name[256];
	uint8_t d_type;
} PORTABLE_DIR_ENTRY;

typedef struct {
	int32_t result;
	int32_t file_type;
	uint64_t child_number;
	HCFS_STAT stat;
} RET_META;

int32_t list_external_volume(char *meta_path,
			     PORTABLE_DIR_ENTRY **ptr_ret_entry,
			     uint64_t *ret_num);

int32_t parse_meta(char *meta_path, RET_META *meta);

int32_t list_dir_inorder(const char *meta_path, const int64_t page_pos,
			 const int32_t start_el, const int32_t limit,
			 int64_t *end_page_pos, int32_t *end_el_no,
			 PORTABLE_DIR_ENTRY *file_list);
    """)

if __name__ == "__main__":
        ffi.compile(verbose=True)