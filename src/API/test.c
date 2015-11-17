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
	char res3[1000];
	char pin_path[100] = "/home/yuxun/mp/test";
	char pin_path2[100] = "/home/yuxun/mp/haha";
	char pin_path3[100] = "/home/yuxun/mp/haha2";
	char pin_path4[100] = "/home/yuxun/mp/haha3";
	char pin_path5[100] = "/home/yuxun/mp/haha4";

	HCFS_unpin_path(res, pin_path);
	printf("%s\n", res);

	HCFS_unpin_path(res2, pin_path2);
	printf("%s\n", res2);

	HCFS_pin_app(res3, pin_path2, pin_path3, pin_path3, pin_path5);
	printf("%s\n", res3);
}


