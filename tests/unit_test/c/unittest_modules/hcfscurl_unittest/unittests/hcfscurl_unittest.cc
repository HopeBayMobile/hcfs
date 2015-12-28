extern "C" {
#include "hcfscurl.h"
#include "fuseop.h"
}
#include <string.h>
#include <gtest/gtest.h>
#include "mock_params.h"


class hcfscurlEnvironment : public ::testing::Environment {
	public:
		void SetUp()
		{
			system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
			memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		}
		void TearDown()
		{
			free(system_config);
		}
};

::testing::Environment* const hcfscurl_env =
	::testing::AddGlobalTestEnvironment(new hcfscurlEnvironment);


class parse_http_header :public ::testing::Test {
protected:
    char httpheader[1000];
    void SetUp()
    {
        const char * http = "HTTP/1.1 200 OK\r\nContent-Length: 1052706\r\nX-Object-Meta-Enc: 1\r\nX-Object-Meta-Comp: 1\r\nX-Object-Meta-Nonce: 11111111111111111111122222222222\r\n\r\n";
        memcpy(httpheader, http, strlen(http));
    }
};

TEST_F(parse_http_header, Parse_Header){
    HCFS_encode_object_meta *object_meta = (HCFS_encode_object_meta *)calloc(1, sizeof(HCFS_encode_object_meta));
    int ret = parse_http_header_coding_meta(object_meta, httpheader, "X-Object-Meta-", "Comp", "Enc", "Nonce");
    printf("%d\n", object_meta->len_enc_session_key);
    EXPECT_EQ(object_meta->comp_alg, 1);
    EXPECT_EQ(object_meta->enc_alg, 1);
    EXPECT_EQ(ret, 0);

}

/*
	Unittest of hcfs_get_auth_swift()
 */
class hcfs_get_auth_swiftTest : public ::testing::Test {
protected:
	char *swift_user;
	char *swift_password;
	char *swift_url;
	CURL_HANDLE *curl_handle;

	void SetUp()
	{
		swift_user = "kewei";
		swift_password = "kewei";
		swift_url = "127.0.0.1";
		SWIFT_PROTOCOL = "https";
		curl_handle = (CURL_HANDLE *)malloc(sizeof(CURL_HANDLE));

		http_perform_retry_fail = FALSE;
		write_auth_header_flag = FALSE;
	}

	void TearDown()
	{
		if (curl_handle)
			 free(curl_handle);
	}
};

TEST_F(hcfs_get_auth_swiftTest, CurlPerformFail)
{
	http_perform_retry_fail = TRUE;
	strcpy(curl_handle->id, "_test_");

	EXPECT_EQ(-1, hcfs_get_auth_swift(swift_user, swift_password,
		swift_url, curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftauth_test_.tmp", F_OK));

	http_perform_retry_fail = FALSE;
}

TEST_F(hcfs_get_auth_swiftTest, ParseHeaderFail)
{
	strcpy(curl_handle->id, "_test_");

	EXPECT_EQ(-1, hcfs_get_auth_swift(swift_user, swift_password,
		swift_url, curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftauth_test_.tmp", F_OK));
}

TEST_F(hcfs_get_auth_swiftTest, GetAuthSuccess)
{
	write_auth_header_flag = TRUE;
	strcpy(curl_handle->id, "_test_");

	EXPECT_EQ(200, hcfs_get_auth_swift(swift_user, swift_password,
		swift_url, curl_handle));

	/* Verify */
	EXPECT_EQ(-1, access("/run/shm/swiftauth_test_.tmp", F_OK));
	EXPECT_STREQ("http://127.0.0.1/fake", swift_url_string);
	EXPECT_STREQ("X-Auth-Token: hello_swift_auth_string",
		swift_auth_string);

	write_auth_header_flag = FALSE;
}
/*
	End of unittest of hcfs_get_auth_swift()
 */

/*
	Unittest of hcfs_init_swift_backend()
 */
class hcfs_init_swift_backendTest : public ::testing::Test {
protected:
	void SetUp()
	{
		http_perform_retry_fail = FALSE;
	}
};

TEST_F(hcfs_init_swift_backendTest, InitBackendGetAuthFail)
{
	CURL_HANDLE curl_handle;

	http_perform_retry_fail = TRUE;
	SWIFT_ACCOUNT = "kewei_account";
	SWIFT_USER = "kewei";
	strcpy(curl_handle.id, "_test_");

	EXPECT_EQ(-1, hcfs_init_swift_backend(&curl_handle));

	http_perform_retry_fail = FALSE;
}
/*
	End of unittest of hcfs_init_swift_backend()
 */

/*
	Unittest of hcfs_list_container()
 */
class hcfs_list_containerTest : public ::testing::Test {
protected:
	CURL_HANDLE *curl_handle;

	void SetUp()
	{
		curl_handle = (CURL_HANDLE *)malloc(sizeof(CURL_HANDLE));
		strcpy(swift_url_string, "https://127.0.0.1/fake_url");
		strcpy(swift_auth_string,
			"X-Auth-Token: hello_swift_auth_string");
		SWIFT_CONTAINER = "test_container";
		S3_BUCKET = "fake_bucket";
		S3_SECRET = "test_secret_key";
		S3_ACCESS = "test_access_key";
		S3_BUCKET_URL = "https://fake_bucket.s3.hicloud.net.tw";

		http_perform_retry_fail = FALSE;
		write_list_header_flag = FALSE;
		let_retry = FALSE;
		hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		if (curl_handle)
			free(curl_handle);
		free(hcfs_system);
	}

};

TEST_F(hcfs_list_containerTest, ListSwift_HttpPerformFail)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	strcpy(curl_handle->id, "_test_");
	http_perform_retry_fail = TRUE;

	EXPECT_EQ(-1, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftlisthead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/swiftlistbody_test_.tmp", F_OK));

	http_perform_retry_fail = FALSE;
}

TEST_F(hcfs_list_containerTest, ListSwift_ParseHeaderFail)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	strcpy(curl_handle->id, "_test_");

	EXPECT_EQ(-1, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftlisthead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/swiftlistbody_test_.tmp", F_OK));
}

TEST_F(hcfs_list_containerTest, ListSwiftSuccess)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	strcpy(curl_handle->id, "_test_");
	write_list_header_flag = TRUE;

	EXPECT_EQ(200, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftlisthead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/swiftlistbody_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
}

TEST_F(hcfs_list_containerTest, ListSwift_Retry_Success) /* Retry list success*/
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	strcpy(curl_handle->id, "_test_");
	write_list_header_flag = TRUE;
	let_retry = TRUE;

	std::cout << "Test retry. wait 10 secs." << std::endl;
	EXPECT_EQ(200, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftlisthead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/swiftlistbody_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
	let_retry = FALSE;
}

TEST_F(hcfs_list_containerTest, ListS3_HttpPerformFail)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	strcpy(curl_handle->id, "_test_");
	http_perform_retry_fail = TRUE;

	EXPECT_EQ(-1, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/S3listhead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/S3listbody_test_.tmp", F_OK));

	http_perform_retry_fail = FALSE;
}

TEST_F(hcfs_list_containerTest, ListS3_ParseHeaderFail)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	strcpy(curl_handle->id, "_test_");

	EXPECT_EQ(-1, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/S3listhead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/S3listbody_test_.tmp", F_OK));
}

TEST_F(hcfs_list_containerTest, ListS3Success)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	strcpy(curl_handle->id, "_test_");
	write_list_header_flag = TRUE;

	EXPECT_EQ(200, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/S3listhead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/S3listbody_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
}

TEST_F(hcfs_list_containerTest, ListS3_Retry_Success) /* Retry list success */
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	strcpy(curl_handle->id, "_test_");
	write_list_header_flag = TRUE;
	let_retry = TRUE;

	std::cout << "Test retry. wait 10 secs." << std::endl;
	EXPECT_EQ(200, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/S3listhead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/S3listbody_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
	let_retry = FALSE;
}
/*
	End of unittest of hcfs_list_container()
 */

/*
	Unittest of hcfs_put_object()
 */
class hcfs_put_objectTest : public ::testing::Test {
protected:
	CURL_HANDLE *curl_handle;
	char *objname;
	char *objpath;
	FILE *fptr;

	void SetUp()
	{
		curl_handle = (CURL_HANDLE *)malloc(sizeof(CURL_HANDLE));
		strcpy(curl_handle->id, "_test_");

		strcpy(swift_url_string, "https://127.0.0.1/fake_url");
		strcpy(swift_auth_string,
			"X-Auth-Token: hello_swift_auth_string");
		SWIFT_CONTAINER = "test_container";
		S3_BUCKET = "fake_bucket";
		S3_SECRET = "test_secret_key";
		S3_ACCESS = "test_access_key";
		S3_BUCKET_URL = "https://fake_bucket.s3.hicloud.net.tw";

		http_perform_retry_fail = FALSE;
		write_list_header_flag = FALSE;
		let_retry = FALSE;

		objname = "here_is_obj";
		objpath = "/tmp/here_is_obj";
		if (!access(objpath, F_OK));
			unlink(objpath);
		fptr = fopen(objpath, "w+");
		hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		sem_init(&(hcfs_system->access_sem), 1, 1);
	}

	void TearDown()
	{
		if (curl_handle)
			free(curl_handle);

		fclose(fptr);
		if (!access(objpath, F_OK));
			unlink(objpath);
		free(hcfs_system);
	}

};

TEST_F(hcfs_put_objectTest, SwiftPutObject_HttpPerformFail)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = S3;
	http_perform_retry_fail = TRUE;

	EXPECT_EQ(-1, hcfs_put_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/swiftputhead_test_.tmp", F_OK));

	http_perform_retry_fail = FALSE;
}

TEST_F(hcfs_put_objectTest, SwiftPutObject_ParseHttpHeaderFail)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;

	EXPECT_EQ(-1, hcfs_put_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/swiftputhead_test_.tmp", F_OK));
}

TEST_F(hcfs_put_objectTest, SwiftPutObjectSuccess)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	write_list_header_flag = TRUE;

	EXPECT_EQ(200, hcfs_put_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/swiftputhead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
}

TEST_F(hcfs_put_objectTest, SwiftPutObject_Retry_Success) /* Retry put success */
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	write_list_header_flag = TRUE;
	let_retry = TRUE;

	std::cout << "Test retry. wait 10 secs." << std::endl;
	EXPECT_EQ(200, hcfs_put_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/swiftputhead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
	let_retry = FALSE;
}

TEST_F(hcfs_put_objectTest, S3PutObject_HttpPerformFail)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	http_perform_retry_fail = TRUE;

	EXPECT_EQ(-1, hcfs_put_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/s3puthead_test_.tmp", F_OK));

	http_perform_retry_fail = FALSE;
}

TEST_F(hcfs_put_objectTest, S3PutObject_ParseHttpHeaderFail)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;

	EXPECT_EQ(-1, hcfs_put_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/s3puthead_test_.tmp", F_OK));
}

TEST_F(hcfs_put_objectTest, S3PutObjectSuccess)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	write_list_header_flag = TRUE;

	EXPECT_EQ(200, hcfs_put_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/s3puthead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
}

TEST_F(hcfs_put_objectTest, S3PutObject_Retry_Success) /* Retry put success */
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	write_list_header_flag = TRUE;
	let_retry = TRUE;

	std::cout << "Test retry. wait 10 secs." << std::endl;
	EXPECT_EQ(200, hcfs_put_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/s3puthead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
	let_retry = FALSE;
}
/*
	End of unittest of hcfs_put_object()
 */

/*
	Unittest of hcfs_get_object()
 */
class hcfs_get_objectTest : public ::testing::Test {
protected:
	CURL_HANDLE *curl_handle;
	char *objname;
	char *objpath;
	FILE *fptr;

	void SetUp()
	{
		curl_handle = (CURL_HANDLE *)malloc(sizeof(CURL_HANDLE));
		strcpy(curl_handle->id, "_test_");

		strcpy(swift_url_string, "https://127.0.0.1/fake_url");
		strcpy(swift_auth_string,
			"X-Auth-Token: hello_swift_auth_string");
		SWIFT_CONTAINER = "test_container";
		S3_BUCKET = "fake_bucket";
		S3_SECRET = "test_secret_key";
		S3_ACCESS = "test_access_key";
		S3_BUCKET_URL = "https://fake_bucket.s3.hicloud.net.tw";

		http_perform_retry_fail = FALSE;
		write_list_header_flag = FALSE;
		let_retry = FALSE;

		objname = "here_is_obj";
		objpath = "/tmp/here_is_obj";
		if (!access(objpath, F_OK));
			unlink(objpath);
		fptr = fopen(objpath, "w+");
		hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		curl_handle->curl_backend = SWIFT;
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		if (curl_handle)
			free(curl_handle);

		fclose(fptr);
		if (!access(objpath, F_OK));
			unlink(objpath);
		free(hcfs_system);
	}
};

TEST_F(hcfs_get_objectTest, SwiftGetObject_HttpPerformFail)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	http_perform_retry_fail = TRUE;

	EXPECT_EQ(-1, hcfs_get_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/swiftgethead_test_.tmp", F_OK));

	http_perform_retry_fail = FALSE;
}

TEST_F(hcfs_get_objectTest, SwiftGetObject_HttpParseHeaderFail)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;

	EXPECT_EQ(-1, hcfs_get_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/swiftgethead_test_.tmp", F_OK));
}

TEST_F(hcfs_get_objectTest, SwiftGetObjectSuccess)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	write_list_header_flag = TRUE;

	EXPECT_EQ(200, hcfs_get_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/swiftgethead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
}

TEST_F(hcfs_get_objectTest, SwiftGetObject_Retry_Success) /* Retry get success */
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	write_list_header_flag = TRUE;
	let_retry = TRUE;

	std::cout << "Test retry. wait 10 secs." << std::endl;
	EXPECT_EQ(200, hcfs_get_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/swiftgethead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
	let_retry = FALSE;
}

TEST_F(hcfs_get_objectTest, S3GetObject_HttpPerformFail)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	http_perform_retry_fail = TRUE;

	EXPECT_EQ(-1, hcfs_get_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/s3gethead_test_.tmp", F_OK));

	http_perform_retry_fail = FALSE;
}

TEST_F(hcfs_get_objectTest, S3GetObject_HttpParseHeaderFail)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;

	EXPECT_EQ(-1, hcfs_get_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/s3gethead_test_.tmp", F_OK));
}

TEST_F(hcfs_get_objectTest, S3GetObjectSuccess)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	write_list_header_flag = TRUE;

	EXPECT_EQ(200, hcfs_get_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/s3gethead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
}

TEST_F(hcfs_get_objectTest, S3GetObject_Retry_Success) /* Retry get success */
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	write_list_header_flag = TRUE;
	let_retry = TRUE;

	std::cout << "Test retry. wait 10 secs." << std::endl;
	EXPECT_EQ(200, hcfs_get_object(fptr, objname, curl_handle, NULL));
	EXPECT_EQ(-1, access("/run/shm/s3gethead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
	let_retry = FALSE;
}
/*
	End of unittest of hcfs_get_object()
 */

/*
	Unittest of hcfs_delete_object()
 */
class hcfs_delete_objectTest : public ::testing::Test {
protected:
	CURL_HANDLE *curl_handle;
	char *objname;

	void SetUp()
	{
		curl_handle = (CURL_HANDLE *)malloc(sizeof(CURL_HANDLE));
		strcpy(curl_handle->id, "_test_");

		strcpy(swift_url_string, "https://127.0.0.1/fake_url");
		strcpy(swift_auth_string,
			"X-Auth-Token: hello_swift_auth_string");
		SWIFT_CONTAINER = "test_container";
		S3_BUCKET = "fake_bucket";
		S3_SECRET = "test_secret_key";
		S3_ACCESS = "test_access_key";
		S3_BUCKET_URL = "https://fake_bucket.s3.hicloud.net.tw";

		http_perform_retry_fail = FALSE;
		write_list_header_flag = FALSE;
		let_retry = FALSE;

		objname = "here_is_obj";
		hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
		hcfs_system->system_going_down = FALSE;
		hcfs_system->backend_is_online = TRUE;
		hcfs_system->sync_manual_switch = ON;
		hcfs_system->sync_paused = OFF;
		sem_init(&(hcfs_system->access_sem), 0, 1);
	}

	void TearDown()
	{
		if (curl_handle)
			free(curl_handle);
		free(hcfs_system);
	}
};

TEST_F(hcfs_delete_objectTest, SwiftDeleteObject_HttpPerformFail)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	http_perform_retry_fail = TRUE;

	EXPECT_EQ(-1, hcfs_delete_object(objname, curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftdeletehead_test_.tmp", F_OK));

	http_perform_retry_fail = FALSE;
}

TEST_F(hcfs_delete_objectTest, SwiftDeleteObject_HttpParseHeaderFail)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;

	EXPECT_EQ(-1, hcfs_delete_object(objname, curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftdeletehead_test_.tmp", F_OK));
}

TEST_F(hcfs_delete_objectTest, SwiftDeleteObjectSuccess)
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	write_list_header_flag = TRUE;

	EXPECT_EQ(200, hcfs_delete_object(objname, curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftdeletehead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
}

TEST_F(hcfs_delete_objectTest, SwiftDeleteObject_Retry_Success) /* Retry delete */
{
	CURRENT_BACKEND = SWIFT;
	curl_handle->curl_backend = SWIFT;
	write_list_header_flag = TRUE;
	let_retry = TRUE;

	std::cout << "Test retry. wait 10 secs." << std::endl;
	EXPECT_EQ(200, hcfs_delete_object(objname, curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftdeletehead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
	let_retry = FALSE;
}

TEST_F(hcfs_delete_objectTest, S3DeleteObject_HttpPerformFail)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	http_perform_retry_fail = TRUE;

	EXPECT_EQ(-1, hcfs_delete_object(objname, curl_handle));
	EXPECT_EQ(-1, access("/run/shm/s3deletehead_test_.tmp", F_OK));

	http_perform_retry_fail = FALSE;
}

TEST_F(hcfs_delete_objectTest, S3DeleteObject_HttpParseHeaderFail)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;

	EXPECT_EQ(-1, hcfs_delete_object(objname, curl_handle));
	EXPECT_EQ(-1, access("/run/shm/s3deletehead_test_.tmp", F_OK));
}

TEST_F(hcfs_delete_objectTest, S3DeleteObjectSuccess)
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	write_list_header_flag = TRUE;

	EXPECT_EQ(200, hcfs_delete_object(objname, curl_handle));
	EXPECT_EQ(-1, access("/run/shm/s3deletehead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
}

TEST_F(hcfs_delete_objectTest, S3DeleteObject_Retry_Success) /* Retry delete*/
{
	CURRENT_BACKEND = S3;
	curl_handle->curl_backend = S3;
	write_list_header_flag = TRUE;
	let_retry = TRUE;

	std::cout << "Test retry. wait 10 secs." << std::endl;
	EXPECT_EQ(200, hcfs_delete_object(objname, curl_handle));
	EXPECT_EQ(-1, access("/run/shm/s3deletehead_test_.tmp", F_OK));

	write_list_header_flag = FALSE;
	let_retry = FALSE;
}

/*
	End of unittest of hcfs_delete_object()
 */

/*
	Unittest of hcfs_init_backend()
 */
TEST(hcfs_init_backendTest, InitSwiftBackendGetAuthFail)
{
	CURL_HANDLE curl_handle;

	CURRENT_BACKEND = SWIFT;
	curl_handle.curl_backend = SWIFT;

	http_perform_retry_fail = TRUE;
	SWIFT_ACCOUNT = "kewei_account";
	SWIFT_USER = "kewei";
	strcpy(curl_handle.id, "_test_");

	EXPECT_EQ(-1, hcfs_init_backend(&curl_handle));

	http_perform_retry_fail = FALSE;
}

TEST(hcfs_init_backendTest, InitS3BackendSuccess)
{
	CURL_HANDLE curl_handle;

	CURRENT_BACKEND = S3;
	curl_handle.curl_backend = S3;
	strcpy(curl_handle.id, "_test_");

	EXPECT_EQ(200, hcfs_init_backend(&curl_handle));
}
/*
	End of unittest of hcfs_init_backend()
 */

/*
	Unittest of hcfs_destroy_backend()
 */
class hcfs_destroy_backendTest : public ::testing::Test {
protected:
	CURL_HANDLE tmphandle;
	void SetUp()
	{
	}

	void TearDown()
	{
		/* Recover */
		swift_destroy = FALSE;
		s3_destroy = FALSE;
	}
};

TEST_F(hcfs_destroy_backendTest, DestroySwift)
{
	swift_destroy = TRUE; /* Destroy swift */
	CURRENT_BACKEND = SWIFT;
	tmphandle.curl_backend = SWIFT;

	hcfs_destroy_backend(&tmphandle);

	EXPECT_EQ(FALSE, swift_destroy);
	EXPECT_EQ(FALSE, s3_destroy);
}

TEST_F(hcfs_destroy_backendTest, DestroyS3)
{
	s3_destroy = TRUE; /* Destroy s3 */
	CURRENT_BACKEND = S3;
	tmphandle.curl_backend = S3;

	hcfs_destroy_backend(&tmphandle);

	EXPECT_EQ(FALSE, swift_destroy);
	EXPECT_EQ(FALSE, s3_destroy);
}
/*
	End of unittest of hcfs_destroy_backend()
 */

/*
	Unittest of hcfs_swift_reauth()
 */
class hcfs_swift_reauthTest : public ::testing::Test {
protected:
	void SetUp()
	{
		http_perform_retry_fail = FALSE;
	}
};

TEST_F(hcfs_swift_reauthTest, BackendAuthFail)
{
	CURL_HANDLE curl_handle;

	http_perform_retry_fail = TRUE;
	SWIFT_ACCOUNT = "kewei_account";
	SWIFT_USER = "kewei";
	strcpy(curl_handle.id, "_test_");
	curl_handle.curl = (CURL *)1;

	EXPECT_EQ(-1, hcfs_swift_reauth(&curl_handle));

	http_perform_retry_fail = FALSE;
}
/*
	End of unittest of hcfs_swift_reauth()
 */

/*
	Unittest of hcfs_S3_reauth()
 */
TEST(hcfs_S3_reauthTest, ReAuthSuccess)
{
	CURL_HANDLE curl_handle;

	strcpy(curl_handle.id, "_test_");
	curl_handle.curl = (CURL *)1;

	EXPECT_EQ(200, hcfs_S3_reauth(&curl_handle));
}
/*
	End of unittest of hcfs_S3_reauth()
 */
