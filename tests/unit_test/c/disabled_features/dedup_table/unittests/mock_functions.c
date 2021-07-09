/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
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
