#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "global.h"
#include "hcfs_sys.h"
#include "enc.h"

void usage()
{
#ifdef DEBUG_MODE
	printf("***Usage - hcfsconf <gen|enc|dec> <config_path>***\n");
#else
	printf("***Usage - hcfsconf <gen> <config_path>***\n");
#endif
}

int _check_file_existed(char *pathname)
{
	if (access(pathname, F_OK) == -1)
		return -1;
	else
		return 0;
}

int _enc_config(unsigned char *output, unsigned char *input,
		long input_len)
{

	int ret;
	unsigned char iv[IV_SIZE] = {0};
	unsigned char *enc_key, *enc_data;

	/* Key and iv */
	enc_key = get_key(PASSPHRASE);
	generate_random_bytes(iv, IV_SIZE);

	enc_data =
		(unsigned char*)malloc(sizeof(unsigned char) * (input_len + TAG_SIZE));

	ret = aes_gcm_encrypt_core(enc_data, input, input_len, enc_key, iv);
	if (ret != 0) {
	        free(enc_data);
	        return -1;
	}

	memcpy(output, iv, IV_SIZE);
	memcpy(output + IV_SIZE, enc_data, input_len + TAG_SIZE);

	free(enc_data);
	return 0;
}

int enc_config(char *config_path)
{
	unsigned char buf[300];
	unsigned char data_buf[1024];
	long data_size = 0;
	int str_len;
	FILE *config = NULL;
	FILE *enc_config = NULL;

	config = fopen(config_path, "r");
	if (config == NULL)
		return -errno;

	while (fgets(buf, sizeof(buf), config) != NULL) {
	        str_len = strlen(buf);
	        memcpy(&(data_buf[data_size]), buf, str_len + 1);
	        data_size += str_len;
	}
	fclose(config);

	unsigned char enc_data[data_size + IV_SIZE + TAG_SIZE];
	if (_enc_config(enc_data, data_buf, data_size) != 0)
		return -errno;

	char *tmp_path =
		(char*)malloc(strlen(config_path) + 5);
	snprintf(tmp_path, strlen(config_path) + 5, "%s.enc", config_path);

	enc_config = fopen(tmp_path, "w");
	if (enc_config == NULL) {
		free(tmp_path);
		return -errno;
	}

	fwrite(enc_data, sizeof(unsigned char),
	       data_size + IV_SIZE + TAG_SIZE, enc_config);
	fclose(enc_config);
	free(tmp_path);

	return 0;
}

int dec_config(char *config_path)
{

	int ret_code = 0;
        long file_size, enc_size, data_size;
        FILE *config = NULL;
        FILE *dec_config = NULL;
        unsigned char *iv_buf = NULL;
        unsigned char *enc_buf = NULL;
        unsigned char *data_buf = NULL;
        unsigned char *enc_key;

        config = fopen(config_path, "r");
        if (config == NULL)
                goto error;

        fseek(config, 0, SEEK_END);
        file_size = ftell(config);
        rewind(config);

        enc_size = file_size - IV_SIZE;
        data_size = enc_size - TAG_SIZE;

        iv_buf = (unsigned char*)malloc(sizeof(char)*IV_SIZE);
        enc_buf = (unsigned char*)malloc(sizeof(char)*(enc_size));
        data_buf = (unsigned char*)malloc(sizeof(char)*(data_size));

        if (!iv_buf || !enc_buf || !data_buf)
                goto error;

        enc_key = get_key(PASSPHRASE);
        fread(iv_buf, sizeof(unsigned char), IV_SIZE, config);
        fread(enc_buf, sizeof(unsigned char), enc_size, config);

        if (aes_gcm_decrypt_core(data_buf, enc_buf, enc_size,
                                 enc_key, iv_buf) != 0) {
                ret_code = -EIO;
		goto end;
	}

	char *tmp_path =
		(char*)malloc(strlen(config_path) + 5);
	snprintf(tmp_path, strlen(config_path) + 5, "%s.dec", config_path);

        dec_config = fopen(tmp_path, "w");
        if (dec_config == NULL)
                goto error;
        fwrite(data_buf, sizeof(unsigned char), data_size,
		dec_config);

	goto end;

error:
	ret_code = -errno;

end:
	if (config)
		fclose(config);
	if (dec_config)
		fclose(dec_config);
	if (data_buf)
		free(data_buf);
	if (enc_buf)
		free(enc_buf);
	if (iv_buf)
		free(iv_buf);

        return ret_code;
}

int gen_config(char *config_path)
{
	unsigned char contents[1000] =
		"METAPATH = /data/hcfs/metastorage\n"
		"BLOCKPATH = /data/hcfs/blockstorage\n"
		"CACHE_SOFT_LIMIT = 20474836480\n"
		"CACHE_HARD_LIMIT = 20548578304\n"
		"CACHE_DELTA = 1048576\n"
		"MAX_BLOCK_SIZE = 1048576\n"
		"CURRENT_BACKEND = NONE\n"
		"SWIFT_ACCOUNT = \n"
		"SWIFT_USER = \n"
		"SWIFT_PASS = \n"
		"SWIFT_URL = \n"
		"SWIFT_CONTAINER = \n"
		"SWIFT_PROTOCOL = http\n"
		"S3_ACCESS = XXXXX\n"
		"S3_SECRET = YYYYY\n"
		"S3_URL = s3.hicloud.net.tw\n"
		"S3_BUCKET = testgateway\n"
		"S3_PROTOCOL = https\n"
		"LOG_LEVEL = 4\n"
		"LOG_PATH = /data";

	if (_check_file_existed(config_path) == 0)
		return -EEXIST;

	long input_len = strlen(contents);
	long output_len = input_len + IV_SIZE + TAG_SIZE;
	unsigned char output[output_len];
	if (_enc_config(output, contents, input_len) != 0)
		return -EIO;

	FILE *config = fopen(config_path, "w");
	if (config == NULL)
		return -errno;

	fwrite(output, sizeof(unsigned char), output_len, config);
	fclose(config);

	return 0;
}

typedef struct {
	const char *name;
	int (*cmd_fn)(char *config_path);
} CMD_INFO;

CMD_INFO cmds[] = {
	{"gen", gen_config},
#ifdef DEBUG_MODE
	{"enc", enc_config},
	{"dec", dec_config},
#endif
};

int main(int argc, char **argv)
{

	int ret_code = 0;
	unsigned int i;

	if (argc != 3) {
		printf("Error - Invalid args\n");
		usage();
		return -EINVAL;
	}

	for (i = 0; i < sizeof(cmds) / sizeof(cmds[0]); i++) {
		if (strcmp(cmds[i].name, argv[1]) == 0) {
			ret_code = cmds[i].cmd_fn(argv[2]);
			goto done;
		}
	}
	printf("Action %s not supported\n", argv[1]);
	usage();
	return -EINVAL;

done:
	if (ret_code != 0) {
		printf("Failed to %s config with path %s\n", argv[1], argv[2]);
		usage();
		return ret_code;
	}

	return 0;
}
