#include "backend_generic.h"
#include "googledrive_curl.h"

int32_t swift_fill_object_info(GOOGLEDRIVE_OBJ_INFO *obj_info,
			       char *objname,
			       char *objectID)
{
	UNUSED(obj_info);
	UNUSED(objname);
	UNUSED(objectID);
	return 0;
}

int32_t swift_download_fill_object_info(GOOGLEDRIVE_OBJ_INFO *obj_info,
			       char *objname,
			       char *objectID)
{
	UNUSED(obj_info);
	UNUSED(objname);
	UNUSED(objectID);
	return 0;
}

int32_t gdrive_fill_object_info(GOOGLEDRIVE_OBJ_INFO *obj_info,
				char *objname,
				char *objectID)
{
	int32_t ret = 0;

	memset(obj_info, 0, sizeof(GOOGLEDRIVE_OBJ_INFO));
	if (objectID && objectID[0] != 0) {
		/* In case that object id exist, just fill in it. It will PUT
		 * object. */
		strcpy(obj_info->fileID, objectID);
	} else {
		/* Otherwise fill obj name and parent id, then it will POST
		 * object */
		strcpy(obj_info->file_title, objname);
		ret = get_parent_id(obj_info->parentID, objname);
	}
	return ret;
}

int32_t gdrive_download_fill_object_info(GOOGLEDRIVE_OBJ_INFO *obj_info,
				char *objname,
				char *objectID)
{
	int32_t ret = 0;

	if (objname == NULL)
		return -EINVAL;

	memset(obj_info, 0, sizeof(GOOGLEDRIVE_OBJ_INFO));
	if (objectID) {
		strncpy(obj_info->fileID, objectID, GDRIVE_ID_LENGTH);
		return 0;
	}

	/* Get object name and parent id */
	strcpy(obj_info->file_title, objname);
	ret = get_parent_id(obj_info->parentID, objname);
	if (ret < 0)
		return ret;
	/* Then fetch object id */
	ret = query_object_id(obj_info);

	return ret;
}

int32_t init_backend_ops(int32_t backend_type)
{
	switch (backend_type) {
	case GOOGLEDRIVE:
		backend_ops.fill_object_info = gdrive_fill_object_info;
		backend_ops.download_fill_object_info =
		    gdrive_download_fill_object_info;
	default:
		backend_ops.fill_object_info = swift_fill_object_info;
		backend_ops.download_fill_object_info =
		    swift_download_fill_object_info;
	}
	return 0;
}


