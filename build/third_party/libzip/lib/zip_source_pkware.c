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

#include "zipint.h"

struct trad_pkware {
    zip_error_t error;
    zip_uint32_t key[3];
};

#define HEADERLEN	12
#define KEY0		305419896
#define KEY1		591751049
#define KEY2		878082192


static void decrypt(struct trad_pkware *, zip_uint8_t *,
		    const zip_uint8_t *, zip_uint64_t, int);
static int decrypt_header(zip_source_t *, struct trad_pkware *);
static zip_int64_t pkware_decrypt(zip_source_t *, void *, void *,
				  zip_uint64_t, zip_source_cmd_t);
static void pkware_free(struct trad_pkware *);


zip_source_t *
zip_source_pkware(zip_t *za, zip_source_t *src,
		  zip_uint16_t em, int flags, const char *password)
{
    struct trad_pkware *ctx;
    zip_source_t *s2;

    if (password == NULL || src == NULL || em != ZIP_EM_TRAD_PKWARE) {
	zip_error_set(&za->error, ZIP_ER_INVAL, 0);
	return NULL;
    }
    if (flags & ZIP_CODEC_ENCODE) {
	zip_error_set(&za->error, ZIP_ER_ENCRNOTSUPP, 0);
	return NULL;
    }

    if ((ctx=(struct trad_pkware *)malloc(sizeof(*ctx))) == NULL) {
	zip_error_set(&za->error, ZIP_ER_MEMORY, 0);
	return NULL;
    }

    zip_error_init(&ctx->error);

    ctx->key[0] = KEY0;
    ctx->key[1] = KEY1;
    ctx->key[2] = KEY2;
    decrypt(ctx, NULL, (const zip_uint8_t *)password, strlen(password), 1);

    if ((s2=zip_source_layered(za, src, pkware_decrypt, ctx)) == NULL) {
	pkware_free(ctx);
	return NULL;
    }

    return s2;
}


static void
decrypt(struct trad_pkware *ctx, zip_uint8_t *out, const zip_uint8_t *in,
	zip_uint64_t len, int update_only)
{
    zip_uint16_t tmp;
    zip_uint64_t i;
    Bytef b;

    for (i=0; i<len; i++) {
	b = in[i];

	if (!update_only) {
	    /* decrypt next byte */
	    tmp = (zip_uint16_t)(ctx->key[2] | 2);
	    tmp = (zip_uint16_t)(((zip_uint32_t)tmp * (tmp ^ 1)) >> 8);
	    b ^= (Bytef)tmp;
	}

	/* store cleartext */
	if (out)
	    out[i] = b;

	/* update keys */
	ctx->key[0] = (zip_uint32_t)crc32(ctx->key[0] ^ 0xffffffffUL, &b, 1) ^ 0xffffffffUL;
	ctx->key[1] = (ctx->key[1] + (ctx->key[0] & 0xff)) * 134775813 + 1;
	b = (Bytef)(ctx->key[1] >> 24);
	ctx->key[2] = (zip_uint32_t)crc32(ctx->key[2] ^ 0xffffffffUL, &b, 1) ^ 0xffffffffUL;
    }
}


static int
decrypt_header(zip_source_t *src, struct trad_pkware *ctx)
{
    zip_uint8_t header[HEADERLEN];
    struct zip_stat st;
    zip_int64_t n;
    unsigned short dostime, dosdate;

    if ((n=zip_source_read(src, header, HEADERLEN)) < 0) {
        _zip_error_set_from_source(&ctx->error, src);
	return -1;
    }
    
    if (n != HEADERLEN) {
        zip_error_set(&ctx->error, ZIP_ER_EOF, 0);
	return -1;
    }

    decrypt(ctx, header, header, HEADERLEN, 0);

    if (zip_source_stat(src, &st) < 0) {
	/* stat failed, skip password validation */
	return 0;
    }

    _zip_u2d_time(st.mtime, &dostime, &dosdate);

    if (header[HEADERLEN-1] != st.crc>>24 && header[HEADERLEN-1] != dostime>>8) {
        zip_error_set(&ctx->error, ZIP_ER_WRONGPASSWD, 0);
	return -1;
    }

    return 0;
}


static zip_int64_t
pkware_decrypt(zip_source_t *src, void *ud, void *data,
	       zip_uint64_t len, zip_source_cmd_t cmd)
{
    struct trad_pkware *ctx;
    zip_int64_t n;

    ctx = (struct trad_pkware *)ud;

    switch (cmd) {
        case ZIP_SOURCE_OPEN:
            if (decrypt_header(src, ctx) < 0)
                return -1;
            return 0;

        case ZIP_SOURCE_READ:
            if ((n=zip_source_read(src, data, len)) < 0) {
                _zip_error_set_from_source(&ctx->error, src);
                return -1;
            }

            decrypt((struct trad_pkware *)ud, (zip_uint8_t *)data, (zip_uint8_t *)data, (zip_uint64_t)n, 0);
            return n;

        case ZIP_SOURCE_CLOSE:
            return 0;

        case ZIP_SOURCE_STAT:
        {
	    zip_stat_t *st;

	    st = (zip_stat_t *)data;

	    st->encryption_method = ZIP_EM_NONE;
	    st->valid |= ZIP_STAT_ENCRYPTION_METHOD;
	    /* TODO: deduce HEADERLEN from size for uncompressed */
	    if (st->valid & ZIP_STAT_COMP_SIZE)
		st->comp_size -= HEADERLEN;
	
            return 0;
        }
            
        case ZIP_SOURCE_SUPPORTS:
            return zip_source_make_command_bitmap(ZIP_SOURCE_OPEN, ZIP_SOURCE_READ, ZIP_SOURCE_CLOSE, ZIP_SOURCE_STAT, ZIP_SOURCE_ERROR, ZIP_SOURCE_FREE, -1);

        case ZIP_SOURCE_ERROR:
            return zip_error_to_data(&ctx->error, data, len);

        case ZIP_SOURCE_FREE:
            pkware_free(ctx);
            return 0;

        default:
            zip_error_set(&ctx->error, ZIP_ER_INVAL, 0);
            return -1;
    }
}


static void
pkware_free(struct trad_pkware *ctx)
{
    free(ctx);
}
