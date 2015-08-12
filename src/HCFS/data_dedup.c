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
#include <unistd.h>
#include <sys/stat.h>
#include <openssl/sha.h>

#include "dedup_table.h"

int compute_hash(char *path, unsigned char *output) {

	unsigned char hash[SHA256_DIGEST_LENGTH];
	FILE *fptr;
	const int buf_size = 16384;
	char *buf;
	int bytes_read;
	SHA256_CTX ctx;

	// Initialize
	SHA256_Init(&ctx);
	buf = malloc(buf_size);
	bytes_read = 0;

	// Open file
	fptr = fopen(path, "r");

	while ((bytes_read = fread(buf, 1, buf_size, fptr))) {
		SHA256_Update(&ctx, buf, bytes_read);
	}

	SHA256_Final(output, &ctx);

	fclose(fptr);
	free(buf);

	return 0;
}


int main(int argc, char* argv[]) {

	char *file_path1 = "dedup_table.h";
	char *file_path2 = "dedup_table.h";
	unsigned char output[SHA256_DIGEST_LENGTH];
	FILE *fptr;
	int fd;
	DDT_BTREE_META this_meta;
	DDT_BTREE_NODE root, result_node;
	int result_idx;
	int ret_val;

	memset(&this_meta, 0, sizeof(DDT_BTREE_META));
	memset(&root, 0, sizeof(DDT_BTREE_NODE));

	// Get hash key
	compute_hash(argv[1], output);

	// To get the file descriptor
	fptr = get_btree_meta(output, &root, &this_meta);
	fd = fileno(fptr);

	// Start to process btree
	ret_val = search_ddt_btree(output, &root, fd, &result_node, &result_idx);
	if (ret_val == 0) {
		printf("Key existed\n");
		// Just increase the refcount of the origin block
		increase_el_refcount(&result_node, result_idx, fd);
	} else {
		// TODO - Wait for block upload finished and insert to btree
		insert_ddt_btree(output, &root, fd, &this_meta);
	}

	// Close file
	fclose(fptr);

	// Re-get the file descriptor
	fptr = get_btree_meta(output, &root, &this_meta);
	fd = fileno(fptr);

	traverse_ddt_btree(&root, fd);

	// Close file
	fclose(fptr);

	return 0;
}
