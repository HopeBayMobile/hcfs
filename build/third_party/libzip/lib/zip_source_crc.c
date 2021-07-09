/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "zipint.h"

struct crc_context {
    int validate; /* whether to check CRC on EOF and return error on mismatch */
    int crc_complete; /* whether CRC was computed for complete file */
    zip_error_t error;
    zip_uint64_t size;
    zip_uint64_t position; /* current reading position */
    zip_uint64_t crc_position; /* how far we've computed the CRC */
    zip_uint32_t crc;
};

static zip_int64_t crc_read(zip_source_t *, void *, void *, zip_uint64_t, zip_source_cmd_t);


zip_source_t *
zip_source_crc(zip_t *za, zip_source_t *src, int validate)
{
    struct crc_context *ctx;

    if (src == NULL) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return NULL;
    }

    if ((ctx=(struct crc_context *)malloc(sizeof(*ctx))) == NULL) {
	zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
	return NULL;
    }

    zip_error_init(&ctx->error);
    ctx->validate = validate;
    ctx->crc_complete = 0;
    ctx->crc_position = 0;
    ctx->crc = (zip_uint32_t)crc32(0, NULL, 0);
    ctx->size = 0;

    return zip_source_layered(za, src, crc_read, ctx);
}


static zip_int64_t
crc_read(zip_source_t *src, void *_ctx, void *data, zip_uint64_t len, zip_source_cmd_t cmd)
{
    struct crc_context *ctx;
    zip_int64_t n;

    ctx = (struct crc_context *)_ctx;

    switch (cmd) {
        case ZIP_SOURCE_OPEN:
            ctx->position = 0;
            return 0;
            
        case ZIP_SOURCE_READ:
            if ((n = zip_source_read(src, data, len)) < 0) {
                _zip_error_set_from_source(&ctx->error, src);
                return -1;
            }
            
            if (n == 0) {
                if (ctx->crc_position == ctx->position) {
                    ctx->crc_complete = 1;
                    ctx->size = ctx->position;

                    if (ctx->validate) {
                        struct zip_stat st;
                    
                        if (zip_source_stat(src, &st) < 0) {
                            _zip_error_set_from_source(&ctx->error, src);
                            return -1;
                        }
                    
                        if ((st.valid & ZIP_STAT_CRC) && st.crc != ctx->crc) {
                            zip_error_set(&ctx->error, ZIP_ER_CRC, 0);
                            return -1;
                        }
                        if ((st.valid & ZIP_STAT_SIZE) && st.size != ctx->size) {
                            zip_error_set(&ctx->error, ZIP_ER_INCONS, 0);
                            return -1;
                        }
                    }
                }
            }
            else if (!ctx->crc_complete && ctx->position <= ctx->crc_position) {
		zip_uint64_t i, nn;

		for (i = ctx->crc_position - ctx->position; i < (zip_uint64_t)n; i += nn) {
		    nn = ZIP_MIN(UINT_MAX, (zip_uint64_t)n-i);

		    ctx->crc = (zip_uint32_t)crc32(ctx->crc, (const Bytef *)data+i, (uInt)nn);
                    ctx->crc_position += nn;
		}
            }
            ctx->position += (zip_uint64_t)n;
            return n;

        case ZIP_SOURCE_CLOSE:
            return 0;

        case ZIP_SOURCE_STAT:
        {
            zip_stat_t *st;

	    st = (zip_stat_t *)data;

	    if (ctx->crc_complete) {
		/* TODO: Set comp_size, comp_method, encryption_method?
		        After all, this only works for uncompressed data. */
		st->size = ctx->size;
		st->crc = ctx->crc;
		st->comp_size = ctx->size;
		st->comp_method = ZIP_CM_STORE;
		st->encryption_method = ZIP_EM_NONE;
		st->valid |= ZIP_STAT_SIZE|ZIP_STAT_CRC|ZIP_STAT_COMP_SIZE|ZIP_STAT_COMP_METHOD|ZIP_STAT_ENCRYPTION_METHOD;;
	    }
            return 0;
        }
            
        case ZIP_SOURCE_ERROR:
            return zip_error_to_data(&ctx->error, data, len);

        case ZIP_SOURCE_FREE:
            free(ctx);
            return 0;
            
        case ZIP_SOURCE_SUPPORTS:
        {
            zip_int64_t mask = zip_source_supports(src);

            if (mask < 0) {
                _zip_error_set_from_source(&ctx->error, src);
                return -1;
            }

            return mask & ~zip_source_make_command_bitmap(ZIP_SOURCE_BEGIN_WRITE, ZIP_SOURCE_COMMIT_WRITE, ZIP_SOURCE_ROLLBACK_WRITE, ZIP_SOURCE_SEEK_WRITE, ZIP_SOURCE_TELL_WRITE, ZIP_SOURCE_REMOVE, -1);
        }

        case ZIP_SOURCE_SEEK:
        {
            zip_int64_t new_position;
            zip_source_args_seek_t *args = ZIP_SOURCE_GET_ARGS(zip_source_args_seek_t, data, len, &ctx->error);

	    if (args == NULL) {
		return -1;
	    }
            if (zip_source_seek(src, args->offset, args->whence) < 0 || (new_position = zip_source_tell(src)) < 0) {
                _zip_error_set_from_source(&ctx->error, src);
                return -1;
            }

            ctx->position = (zip_uint64_t)new_position;

            return 0;
        }

        case ZIP_SOURCE_TELL:
            return (zip_int64_t)ctx->position;
            
        default:
            zip_error_set(&ctx->error, ZIP_ER_OPNOTSUPP, 0);
            return -1;
    }
}
