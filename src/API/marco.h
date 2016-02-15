#ifndef GW20_API_MARCO_H_
#define GW20_API_MARCO_H_

#define CONCAT_ARGS(A)\
	{\
		if (A != NULL) {\
			str_len = strlen(A) + 1;\
			memcpy(&(buf[cmd_len]), &str_len, sizeof(ssize_t));\
			cmd_len += sizeof(ssize_t);\
			memcpy(&(buf[cmd_len]), A, str_len);\
			cmd_len += str_len;\
		} \
	}

#define CONCAT_LL_ARGS(A)\
	{\
		memcpy(&(res_buf[ret_len]), &A, sizeof(long long));\
		ret_len += sizeof(long long);\
	}

#define READ_LL_ARGS(A)\
	{\
		memcpy(&A, &(buf[buf_idx]), sizeof(long long));\
		buf_idx += sizeof(long long);\
	}

#endif  /* GW20_API_MARCO_H_ */
