/************************************************************************r
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: dedup_table.c
* Abstract: The c source code file for data deduplication table.
*
* Revision History
* 2015/07/17 Yuxun create this file
*
**************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>

int compute_hash(char *path, unsigned char *output) {

	unsigned char hash[SHA256_DIGEST_LENGTH];
	FILE *ptr;
	const int buf_size = 16384;
	char *buf;
	int bytes_read;
	SHA256_CTX ctx;

	// Initialize
	SHA256_Init(&ctx);
	buf = malloc(buf_size);
	bytes_read = 0;

	// Open file
	ptr = fopen(path, "r");

	while ((bytes_read = fread(buf, 1, buf_size, ptr))) {
		SHA256_Update(&ctx, buf, bytes_read);
	}

	SHA256_Final(output, &ctx);

	fclose(ptr);
	free(buf);

	return 0;
}

