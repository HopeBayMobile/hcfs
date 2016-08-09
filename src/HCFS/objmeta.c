#include "objmeta.h"

HTTP_meta *new_http_meta(void)
{
	HTTP_meta *meta = (HTTP_meta *)calloc(1, sizeof(HTTP_meta));

	meta->count = 0;
	meta->data = NULL;
	return meta;
}

void delete_http_meta(HTTP_meta *meta)
{
	if (!meta)
		return;

	if (meta->data != NULL) {
		int32_t i;

		for (i = 0; i < 2 * (meta->count); i++) {
			free(meta->data[i]);
		}
		free(meta->data);
	}
}

int32_t transform_objdata_to_header(HTTP_meta *meta,
				HCFS_encode_object_meta *encode_meta)
{
	if (!encode_meta || !meta) {
		return 1;
	}
	meta->count = 3;
	meta->data = (char **)calloc(2 * 3, sizeof(char *));

	meta->data[0] = (char *)calloc(10, sizeof(char));
	meta->data[1] = (char *)calloc(3, sizeof(char));
	meta->data[2] = (char *)calloc(10, sizeof(char));
	meta->data[3] = (char *)calloc(3, sizeof(char));
	meta->data[4] = (char *)calloc(10, sizeof(char));
	meta->data[5] =
	    (char *)calloc((encode_meta->len_enc_session_key) + 1, sizeof(char));

	sprintf(meta->data[0], "%s", "comp");
	sprintf(meta->data[1], "%d", encode_meta->comp_alg);
	sprintf(meta->data[2], "%s", "enc");
	sprintf(meta->data[3], "%d", encode_meta->enc_alg);
	sprintf(meta->data[4], "%s", "nonce");
	sprintf(meta->data[5], "%s", encode_meta->enc_session_key);

	return 0;
}
