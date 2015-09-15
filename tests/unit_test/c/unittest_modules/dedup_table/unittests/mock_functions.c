#include <unistd.h>
#include <errno.h>

#include "macro.h"


#define METAPATHLEN 400

int write_log(int level, char *format, ...)
{
	return 0;
}

int fetch_ddt_path(char *pathname, unsigned char last_char)
{
        char metapath[METAPATHLEN] = "testpatterns/";
        char tempname[METAPATHLEN];
        int errcode, ret;

        if (metapath == NULL)
                return -1;

        /* Creates meta path if it does not exist */
        if (access(metapath, F_OK) == -1)
                MKDIR(metapath, 0700);

        snprintf(tempname, METAPATHLEN, "%s/ddt", metapath);

        /* Creates meta path for meta subfolder if it does not exist */
        if (access(tempname, F_OK) == -1)
                MKDIR(tempname, 0700);

        snprintf(pathname, METAPATHLEN, "%s/ddt/ddt_meta_%02x",
                metapath, last_char);

        return 0;
errcode_handle:
        return errcode;
}
