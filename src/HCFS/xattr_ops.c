#include <errno.h>
#include "xattr_ops.h"
#include "string.h"

int parse_xattr_namespace(const char *name, char *name_space, char *key)
{
	int index;
	int key_len;
	char namespace_string[20];

	/* Find '.' which is used to combine namespace and key.
	   eg.: [namespace].[key] */
	index = 0;
	while (name[index]) {
		if (name[index] == '.') /* Find '.' */
			break;
		index++;
	}
	
	if (name[index] != '.') /* No character '.', invalid args. */
		return -EINVAL;
	
	key_len = strlen(name) - (index + 1);
	if ((key_len >= MAX_KEY_SIZE) || (key_len <= 0)) /* key len is invalid. */
		return -EINVAL;

	memcpy(namespace_string, name, sizeof(char) * index); /* Copy namespace */
	namespace_string[index] = '\0';
	memcpy(key, &name[index+1], sizeof(char) * key_len); /* Copy key */
	key[key_len] = '\0';
		
	if (!strcmp("user", namespace_string)) {
		*name_space = USER;
		return 0;
	} else if(!strcmp("system", namespace_string)) {
		*name_space = SYSTEM;
		return 0;
	} else if(!strcmp("security", namespace_string)) {
		*name_space = SECURITY;
		return 0;
	} else if(!strcmp("trusted", namespace_string)) {
		*name_space = TRUSTED;
		return 0;
	} else {	
		return -EINVAL; /* Namespace is not supported. */
	}

	return 0;
}
