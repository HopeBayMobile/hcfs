#ifndef GW20_HCFS_OBJDATA_H_
#define GW20_HCFS_OBJDATA_H_

#include "enc.h"

typedef struct {
	char **data;
	int32_t count;
} HTTP_meta;

HTTP_meta *new_http_meta(void);

void delete_http_meta(HTTP_meta *);

int32_t transform_objdata_to_header(HTTP_meta *meta,
				HCFS_encode_object_meta *encode_meta);

#endif
