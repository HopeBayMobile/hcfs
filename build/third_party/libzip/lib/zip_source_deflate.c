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

struct deflate {
    zip_error_t error;
    
    bool eof;
    bool can_store;
    bool is_stored;
    int mem_level;
    zip_uint64_t size;
    zip_uint8_t buffer[BUFSIZE];
    z_stream zstr;
};

static zip_int64_t compress_read(zip_source_t *, struct deflate *, void *, zip_uint64_t);
static zip_int64_t decompress_read(zip_source_t *, struct deflate *, void *, zip_uint64_t);
static zip_int64_t deflate_compress(zip_source_t *, void *, void *, zip_uint64_t, zip_source_cmd_t);
static zip_int64_t deflate_decompress(zip_source_t *, void *, void *, zip_uint64_t, zip_source_cmd_t);
static void deflate_free(struct deflate *);


zip_source_t *
zip_source_deflate(zip_t *za, zip_source_t *src, zip_int32_t cm, int flags)
{
    struct deflate *ctx;
    zip_source_t *s2;

    if (src == NULL || (cm != ZIP_CM_DEFLATE && !ZIP_CM_IS_DEFAULT(cm))) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return NULL;
    }

    if ((ctx=(struct deflate *)malloc(sizeof(*ctx))) == NULL) {
	zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
	return NULL;
    }

    zip_error_init(&ctx->error);
    ctx->eof = false;
    ctx->is_stored = false;
    ctx->can_store = ZIP_CM_IS_DEFAULT(cm);
    if (flags & ZIP_CODEC_ENCODE) {
	ctx->mem_level = MAX_MEM_LEVEL;
    }

    if ((s2=zip_source_layered(za, src,
			       ((flags & ZIP_CODEC_ENCODE)
				? deflate_compress : deflate_decompress),
			       ctx)) == NULL) {
	deflate_free(ctx);
	return NULL;
    }

    return s2;
}


static zip_int64_t
compress_read(zip_source_t *src, struct deflate *ctx, void *data, zip_uint64_t len)
{
    int end, ret;
    zip_int64_t n;
    zip_uint64_t out_offset;
    uInt out_len;

    if (zip_error_code_zip(&ctx->error) != ZIP_ER_OK)
	return -1;
    
    if (len == 0 || ctx->is_stored) {
	return 0;
    }
	
    out_offset = 0;
    out_len = (uInt)ZIP_MIN(UINT_MAX, len);
    ctx->zstr.next_out = (Bytef *)data;
    ctx->zstr.avail_out = out_len;

    end = 0;
    while (!end) {
	ret = deflate(&ctx->zstr, ctx->eof ? Z_FINISH : 0);

	switch (ret) {
        case Z_STREAM_END:
            if (ctx->can_store && ctx->zstr.total_in <= ctx->zstr.total_out) {
                ctx->is_stored = true;
                ctx->size = ctx->zstr.total_in;
                memcpy(data, ctx->buffer, ctx->size);
                return (zip_int64_t)ctx->size;
            }
            /* fallthrough */
	case Z_OK:
	    /* all ok */

	    if (ctx->zstr.avail_out == 0) {
		out_offset += out_len;
		if (out_offset < len) {
		    out_len = (uInt)ZIP_MIN(UINT_MAX, len-out_offset);
		    ctx->zstr.next_out = (Bytef *)data+out_offset;
		    ctx->zstr.avail_out = out_len;
		}
		else {
                    ctx->can_store = false;
		    end = 1;
		}
	    }
	    else if (ctx->eof && ctx->zstr.avail_in == 0)
		end = 1;
	    break;

	case Z_BUF_ERROR:
	    if (ctx->zstr.avail_in == 0) {
		if (ctx->eof) {
		    end = 1;
		    break;
		}

		if ((n=zip_source_read(src, ctx->buffer, sizeof(ctx->buffer))) < 0) {
                    _zip_error_set_from_source(&ctx->error, src);
		    end = 1;
		    break;
		}
		else if (n == 0) {
		    ctx->eof = true;
		    /* TODO: check against stat of src? */
		    ctx->size = ctx->zstr.total_in;
		}
		else {
                    if (ctx->zstr.total_in > 0) {
                        /* we overwrote a previously filled ctx->buffer */
                        ctx->can_store = false;
                    }
		    ctx->zstr.next_in = (Bytef *)ctx->buffer;
		    ctx->zstr.avail_in = (uInt)n;
		}
		continue;
	    }
	    /* fallthrough */
	case Z_NEED_DICT:
	case Z_DATA_ERROR:
	case Z_STREAM_ERROR:
	case Z_MEM_ERROR:
            zip_error_set(&ctx->error, ZIP_ER_ZLIB, ret);

	    end = 1;
	    break;
	}
    }

    if (ctx->zstr.avail_out < len) {
	ctx->can_store = false;
	return (zip_int64_t)(len - ctx->zstr.avail_out);
    }

    return (zip_error_code_zip(&ctx->error) == ZIP_ER_OK) ? 0 : -1;
}


static zip_int64_t
decompress_read(zip_source_t *src, struct deflate *ctx, void *data, zip_uint64_t len)
{
    int end, ret;
    zip_int64_t n;
    zip_uint64_t out_offset;
    uInt out_len;

    if (zip_error_code_zip(&ctx->error) != ZIP_ER_OK)
	return -1;
    
    if (len == 0)
	return 0;

    out_offset = 0;
    out_len = (uInt)ZIP_MIN(UINT_MAX, len);
    ctx->zstr.next_out = (Bytef *)data;
    ctx->zstr.avail_out = out_len;

    end = 0;
    while (!end) {
	ret = inflate(&ctx->zstr, Z_SYNC_FLUSH);

	switch (ret) {
	case Z_OK:
	    if (ctx->zstr.avail_out == 0) {
		out_offset += out_len;
		if (out_offset < len) {
		    out_len = (uInt)ZIP_MIN(UINT_MAX, len-out_offset);
		    ctx->zstr.next_out = (Bytef *)data+out_offset;
		    ctx->zstr.avail_out = out_len;
		}
		else {
		    end = 1;
		}
	    }
	    break;
	    
	case Z_STREAM_END:
	    ctx->eof = 1;
	    end = 1;
	    break;

	case Z_BUF_ERROR:
	    if (ctx->zstr.avail_in == 0) {
		if (ctx->eof) {
		    end = 1;
		    break;
		}

		if ((n=zip_source_read(src, ctx->buffer, sizeof(ctx->buffer))) < 0) {
                    _zip_error_set_from_source(&ctx->error, src);
		    end = 1;
		    break;
		}
		else if (n == 0) {
		    ctx->eof = 1;
		}
		else {
		    ctx->zstr.next_in = (Bytef *)ctx->buffer;
		    ctx->zstr.avail_in = (uInt)n;
		}
		continue;
	    }
	    /* fallthrough */
	case Z_NEED_DICT:
	case Z_DATA_ERROR:
	case Z_STREAM_ERROR:
	case Z_MEM_ERROR:
            zip_error_set(&ctx->error, ZIP_ER_ZLIB, ret);
	    end = 1;
	    break;
	}
    }

    if (ctx->zstr.avail_out < len)
	return (zip_int64_t)(len - ctx->zstr.avail_out);

    return (zip_error_code_zip(&ctx->error) == ZIP_ER_OK) ? 0 : -1;
}


static zip_int64_t
deflate_compress(zip_source_t *src, void *ud, void *data, zip_uint64_t len, zip_source_cmd_t cmd)
{
    struct deflate *ctx;
    int ret;

    ctx = (struct deflate *)ud;

    switch (cmd) {
    case ZIP_SOURCE_OPEN:
	ctx->zstr.zalloc = Z_NULL;
	ctx->zstr.zfree = Z_NULL;
	ctx->zstr.opaque = NULL;
	ctx->zstr.avail_in = 0;
	ctx->zstr.next_in = NULL;
	ctx->zstr.avail_out = 0;
	ctx->zstr.next_out = NULL;

	/* negative value to tell zlib not to write a header */
	if ((ret=deflateInit2(&ctx->zstr, Z_BEST_COMPRESSION, Z_DEFLATED, -MAX_WBITS, ctx->mem_level, Z_DEFAULT_STRATEGY)) != Z_OK) {
            zip_error_set(&ctx->error, ZIP_ER_ZLIB, ret);
	    return -1;
	}

	return 0;

    case ZIP_SOURCE_READ:
	return compress_read(src, ctx, data, len);

    case ZIP_SOURCE_CLOSE:
	deflateEnd(&ctx->zstr);
	return 0;

    case ZIP_SOURCE_STAT:
    	{
	    zip_stat_t *st;

	    st = (zip_stat_t *)data;

	    st->comp_method = ctx->is_stored ? ZIP_CM_STORE : ZIP_CM_DEFLATE;
	    st->valid |= ZIP_STAT_COMP_METHOD;
	    if (ctx->eof) {
		st->comp_size = ctx->size;
		st->valid |= ZIP_STAT_COMP_SIZE;
	    }
	    else
		st->valid &= ~ZIP_STAT_COMP_SIZE;
	}
	return 0;

    case ZIP_SOURCE_ERROR:
        return zip_error_to_data(&ctx->error, data, len);

    case ZIP_SOURCE_FREE:
	deflate_free(ctx);
	return 0;

    case ZIP_SOURCE_SUPPORTS:
        return ZIP_SOURCE_SUPPORTS_READABLE;
            
    default:
        zip_error_set(&ctx->error, ZIP_ER_INTERNAL, 0);
	return -1;
    }
}


static zip_int64_t
deflate_decompress(zip_source_t *src, void *ud, void *data,
		   zip_uint64_t len, zip_source_cmd_t cmd)
{
    struct deflate *ctx;
    zip_int64_t n;
    int ret;

    ctx = (struct deflate *)ud;

    switch (cmd) {
        case ZIP_SOURCE_OPEN:
            if ((n=zip_source_read(src, ctx->buffer, sizeof(ctx->buffer))) < 0) {
                _zip_error_set_from_source(&ctx->error, src);
                return -1;
            }

            ctx->zstr.zalloc = Z_NULL;
            ctx->zstr.zfree = Z_NULL;
            ctx->zstr.opaque = NULL;
            ctx->zstr.next_in = (Bytef *)ctx->buffer;
            ctx->zstr.avail_in = (uInt)n;

            /* negative value to tell zlib that there is no header */
            if ((ret=inflateInit2(&ctx->zstr, -MAX_WBITS)) != Z_OK) {
                zip_error_set(&ctx->error, ZIP_ER_ZLIB, ret);
                return -1;
            }
            return 0;

        case ZIP_SOURCE_READ:
            return decompress_read(src, ctx, data, len);

        case ZIP_SOURCE_CLOSE:
            inflateEnd(&ctx->zstr);
            return 0;

        case ZIP_SOURCE_STAT:
        {
            zip_stat_t *st;
            
            st = (zip_stat_t *)data;
            
            st->comp_method = ZIP_CM_STORE;
            if (st->comp_size > 0 && st->size > 0)
                st->comp_size = st->size;
            
            return 0;
        }

        case ZIP_SOURCE_ERROR:
            return zip_error_to_data(&ctx->error, data, len);

        case ZIP_SOURCE_FREE:
            free(ctx);
            return 0;
            
        case ZIP_SOURCE_SUPPORTS:
            return zip_source_make_command_bitmap(ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE, ZIP_SOURCE_STAT, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, -1);

        default:
            zip_error_set(&ctx->error, ZIP_ER_OPNOTSUPP, 0);
            return -1;
    }
}


static void
deflate_free(struct deflate *ctx)
{
    free(ctx);
}
