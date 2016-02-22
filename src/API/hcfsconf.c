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
	printf("***Usage - hcfsconf <enc|dec> <config_path>***\n");
}

int _enc_config(unsigned char *output, unsigned char *input,
		long input_len)
{

	int ret;
	unsigned char iv[IV_SIZE] = {0};
	unsigned char *enc_key, *enc_data;
	long data_size, enc_size;

	/* Key and iv */
	enc_key = get_key(PASSPHRASE);
	generate_random_bytes(iv, IV_SIZE);

	enc_data =
		(char*)malloc(sizeof(unsigned char) * (input_len + TAG_SIZE));

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
	char buf[300];
	char data_buf[1024];
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

	unsigned char *tmp_path =
		(unsigned char*)malloc(strlen(config_path) + 5);
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

        iv_buf = (char*)malloc(sizeof(char)*IV_SIZE);
        enc_buf = (char*)malloc(sizeof(char)*(enc_size));
        data_buf = (char*)malloc(sizeof(char)*(data_size));

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

	unsigned char *tmp_path =
		(unsigned char*)malloc(strlen(config_path) + 5);
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

int main(int argc, char **argv)
{

	int ret_code = 0;

	if (argc != 3) {
		printf("Error - Invalid args\n");
		usage();
		return -EINVAL;
	}

	if (access(argv[2], F_OK|R_OK) == -1) {
		printf("Error - Config path not found.\n");
		usage();
		return -ENOENT;
	}


	if (strcmp(argv[1], "enc") == 0) {
		ret_code = enc_config(argv[2]);
		if (ret_code != 0) {
			printf("Error - Failed to encrypt config.\n");
			return ret_code;
		}

	} else if (strcmp(argv[1], "dec") == 0) {
		ret_code = dec_config(argv[2]);
		if (ret_code != 0) {
			printf("Error - Failed to decrypt config.\n");
			return ret_code;
		}

	} else {
		printf("Error - Action not supported.\n");
		usage();
		return -EINVAL;
	}

	return 0;
}
