#include "backend_generic.h"
#include "googledrive_curl.h"
#include "do_restoration.h"

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

int32_t swift_get_pkglist_id(char *id)
{
	UNUSED(id);
	return 0;
}

int32_t gdrive_get_pkglist_id(char *id)
{
	char pkglist_id_filename[200];
	int32_t ret = 0;
	size_t ret_size = 0, id_len;
	FILE *fptr;

	if (pkg_backup_data.pkglist_id[0]) {
		strncpy(id, pkg_backup_data.pkglist_id,
			sizeof(pkg_backup_data.pkglist_id) - 1);
		return 0;
	}

	LOCK_PKG_BACKUP_SEM();
	/* Try to fetch from cache */
	if (pkg_backup_data.pkglist_id[0]) {
		strncpy(id, pkg_backup_data.pkglist_id,
			sizeof(pkg_backup_data.pkglist_id) - 1);
		goto out;
	}

	/* Try to fetch from file */
	if (hcfs_system->system_restoring == RESTORING_STAGE1)
		snprintf(pkglist_id_filename, sizeof(pkglist_id_filename) - 1,
			 "%s/pkglist_id", RESTORE_METAPATH);
	else
		snprintf(pkglist_id_filename, sizeof(pkglist_id_filename) - 1,
			 "%s/pkglist_id", METAPATH);

	fptr = fopen(pkglist_id_filename, "r");
	if (!fptr) {
		ret = -errno;
		goto out;
	}
	fseek(fptr, 0, SEEK_END);
	id_len = ftell(fptr);
	fseek(fptr, 0, SEEK_SET);
	ret_size = fread(pkg_backup_data.pkglist_id, 1, id_len, fptr);
	if (ret_size < id_len) {
		ret = -errno;
		write_log(0,
			  "Error: Fail to read pkglist id from file. Code %d",
			  -ret);
		fclose(fptr);
		unlink(pkglist_id_filename);
		goto out;
	}
	fclose(fptr);
	strncpy(id, pkg_backup_data.pkglist_id,
		sizeof(pkg_backup_data.pkglist_id) - 1);
out:
	UNLOCK_PKG_BACKUP_SEM();
	return ret;
}

int32_t swift_record_pkglist_id(const char *id)
{
	UNUSED(id);
	return 0;
}

int32_t gdrive_record_pkglist_id(const char *id)
{
	char pkglist_id_filename[200];
	int32_t ret = 0;
	int32_t id_len = 0;
	FILE *fptr;

	LOCK_PKG_BACKUP_SEM();
	if (hcfs_system->system_restoring == RESTORING_STAGE1)
		snprintf(pkglist_id_filename, sizeof(pkglist_id_filename) - 1,
			 "%s/pkglist_id", RESTORE_METAPATH);
	else
		snprintf(pkglist_id_filename, sizeof(pkglist_id_filename) - 1,
			 "%s/pkglist_id", METAPATH);
	id_len = strlen(id);
	fptr = fopen(pkglist_id_filename, "w+");
	if (!fptr) {
		ret = -errno;
		goto out;
	}
	fseek(fptr, 0, SEEK_SET);
	fwrite(id, 1, id_len, fptr);
	fclose(fptr);
	/* Copy to cache */
	strncpy(pkg_backup_data.pkglist_id, id,
		sizeof(pkg_backup_data.pkglist_id) - 1);
out:
	UNLOCK_PKG_BACKUP_SEM();
	return ret;
}

int32_t init_backend_ops(int32_t backend_type)
{
	switch (backend_type) {
	case GOOGLEDRIVE:
		backend_ops.fill_object_info = gdrive_fill_object_info;
		backend_ops.download_fill_object_info =
		    gdrive_download_fill_object_info;
		backend_ops.get_pkglist_id = gdrive_get_pkglist_id;
		backend_ops.record_pkglist_id = gdrive_record_pkglist_id;
		break;
	default:
		backend_ops.fill_object_info = swift_fill_object_info;
		backend_ops.download_fill_object_info =
		    swift_download_fill_object_info;
		backend_ops.get_pkglist_id = swift_get_pkglist_id;
		backend_ops.record_pkglist_id = swift_record_pkglist_id;
		break;
	}
	return 0;
}


