#ifndef GW20_HCFS_BACKEND_OPERATION_H_
#define GW20_HCFS_BACKEND_OPERATION_H_

#include "hcfs_tocloud.h"

typedef struct {
	int32_t (*fill_object_info)(GOOGLEDRIVE_OBJ_INFO *obj_info,
				    char *objname,
				    char *objectID);
	int32_t (*download_fill_object_info)(GOOGLEDRIVE_OBJ_INFO *obj_info,
					  char *objname,
					  char *objectID);
	int32_t (*get_pkglist_id)(char *id);
	int32_t (*record_pkglist_id)(const char *id);
} BACKEND_OPERATION;

BACKEND_OPERATION backend_ops;

int32_t init_backend_ops(int32_t backend_type);

int32_t swift_fill_object_info(GOOGLEDRIVE_OBJ_INFO *obj_info,
			       char *objname,
			       char *objectID);

int32_t gdrive_fill_object_info(GOOGLEDRIVE_OBJ_INFO *obj_info,
				char *objname,
				char *objectID);

#endif
