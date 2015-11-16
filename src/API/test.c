#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <jansson.h>

#include "HCFS_api.h"
#include "pin_ops.h"
#include "global.h"

int main()
{

	char res[1000];
	char res2[1000];
	char pin_path[100] = "/home/yuxun/mp/test";
	char pin_path_2[100] = "/home/yuxun/mp/haha";

	HCFS_pin_path(res, pin_path);
	printf("%s\n", res);

	HCFS_pin_path(res2, pin_path_2);
	printf("%s", res2);
}


