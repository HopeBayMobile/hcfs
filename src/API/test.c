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

	long long cloud_usage = 0;
	get_cloud_usage(&cloud_usage);
	printf("%lld\n", cloud_usage);

	long long cache_total = 0;
	long long cache_used = 0;
	long long cache_dirty = 0;
	get_cache_usage(&cache_total, &cache_used, &cache_dirty);
	printf("%lld\n", cache_total);
	printf("%lld\n", cache_used);
	printf("%lld\n", cache_dirty);

	long long pin_max = 0;
	long long pin_total = 0;
	get_pin_usage(&pin_max, &pin_total);
	printf("%lld\n", pin_max);
	printf("%lld\n", pin_total);

	char *res4;
	HCFS_pin_status(&res4, pin_path);
	printf("%s\n", res4);
	free(res4);

	char *res5;
	HCFS_file_status(&res5, pin_path);
	printf("%s\n", res5);
	free(res5);
}


