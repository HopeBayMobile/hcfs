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
#include "hcfs_stat.h"
#include "global.h"

int main()
{

	char pin_path[100] = "/home/yuxun/mp/test";
	char pin_path2[100] = "/home/yuxun/mp/haha";
	char pin_path3[100] = "/home/yuxun/mp/haha2";
	char pin_path4[100] = "/home/yuxun/mp/haha3";
	char pin_path5[100] = "/home/yuxun/mp/haha4";

	char *res1;
	HCFS_pin_path(&res1, pin_path);
	printf("%s\n", res1);
	free(res1);

	char *res2;
	HCFS_unpin_path(&res2, pin_path2);
	printf("%s\n", res2);
	free(res2);

	char *res3;
	HCFS_unpin_app(&res3, pin_path2, pin_path3, pin_path3, pin_path5);
	printf("%s\n", res3);
	free(res3);

	char *res4;
	HCFS_pin_status(&res4, pin_path);
	printf("%s\n", res4);
	free(res4);

	char *res5;
	HCFS_file_status(&res5, pin_path);
	printf("%s\n", res5);
	free(res5);

	char *res6;
	HCFS_stat(&res6);
	printf("%s\n", res6);
	free(res6);

	char *res7;
	HCFS_dir_status(&res7, pin_path2);
	printf("%s\n", res7);
	free(res7);

	char *res8;
	HCFS_set_config(&res8, "swift_account", "test");
	printf("res8 - %s\n", res8);
	free(res8);

	char *res9;
	HCFS_get_config(&res9, "swift_url");
	printf("res9 - %s\n", res9);
	free(res9);
}


