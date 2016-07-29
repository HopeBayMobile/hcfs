/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: test.c
* Abstract: This c source file for HCFSAPI test script.
*
* Revision History
* 2016/5/27 Modified after first code review.
*
**************************************************************************/

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

int32_t main()
{

	char pin_path[100] = "/home/yuxun/mp/test1";
	char pin_path2[100] = "/home/yuxun/mp/test2";
	char pin_path3[100] = "/home/yuxun/mp/haha2";
	char pin_path4[100] = "/home/yuxun/mp/haha3";
	char pin_path5[100] = "/home/yuxun/mp/haha4";

	//char *res1;
	//HCFS_pin_path(&res1, pin_path, 1);
	//printf("pin path - %s\n", res1);
	//free(res1);

	//char *res2;
	//HCFS_pin_path(&res2, pin_path2, 2);
	//printf("high-pri-pin path - %s\n", res2);
	//free(res2);

	//char *res3;
	//HCFS_unpin_path(&res3, pin_path2);
	//printf("unpin path - %s\n", res3);
	//free(res3);

	//char *res4;
	//HCFS_pin_status(&res4, pin_path2);
	//printf("pin status - %s\n", res4);
	//free(res4);

	//char *res5;
	//HCFS_file_status(&res5, pin_path);
	//printf("file status - %s\n", res5);
	//free(res5);

	//char *res6;
	//HCFS_stat(&res6);
	//printf("stat - %s\n", res6);
	//free(res6);

	//char *res7;
	//HCFS_dir_status(&res7, pin_path2);
	//printf("dir stat - %s\n", res7);
	//free(res7);

	//char *res8;
	//char key[500] = "swift_url";
	//char value[500] = "10.10.10.10:8080";
	//HCFS_set_config(&res8, key, value);
	//printf("set config - %s\n", res8);
	//free(res8);

	//char *res9;
	//HCFS_get_config(&res9, "swift_url");
	//printf("get config - %s\n", res9);
	//free(res9);

	//char *res10;
	//HCFS_reset_xfer(&res10);
	//printf("reset xfer - %s\n", res10);
	//free(res10);

	//char *res11;
	//HCFS_toggle_sync(&res11, 1);
	//printf("toggle sync - %s\n", res11);
	//free(res11);

	//char *res12;
	//HCFS_get_sync_status(&res12);
	//printf("sync status - %s\n", res12);
	//free(res12);

	//char *res13;
	//HCFS_get_occupied_size(&res13);
	//printf("occupied size - %s\n", res13);
	//free(res13);

	char *res14;
	HCFS_set_notify_server(&res14, "event.notify.mock.server");
	printf("set notify server - %s\n", res14);
	free(res14);

	char *res15;
	HCFS_set_swift_token(&res15,
			"http://127.0.0.1:12345/v1/AUTH_test",
			"AUTH_tk883a4aeb1b1a4b0e87f45ccf1993f7fe");
	printf("set swift token - %s\n", res15);
	free(res15);
}


