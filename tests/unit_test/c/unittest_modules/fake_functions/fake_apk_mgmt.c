#include "macro.h"
int32_t toggle_use_minimal_apk(_UNUSED bool new_val) { return 0; }

int32_t initialize_minimal_apk(void) { return 0; }

int32_t terminate_minimal_apk(void) { return 0; }

int32_t create_minapk_table(void) { return 0; }

int32_t destroy_minapk_table(void) { return 0; }

int32_t insert_minapk_data(_UNUSED ino_t parent_ino,
			   _UNUSED const char *apk_name,
			   _UNUSED ino_t minapk_ino)
{
	return 0;
}

int32_t query_minapk_data(_UNUSED ino_t parent_ino,
			  _UNUSED const char *apk_name,
			  _UNUSED ino_t *minapk_ino)
{
	return 0;
}

int32_t remove_minapk_data(_UNUSED ino_t parent_ino,
			   _UNUSED const char *apk_name)
{
	return 0;
}
