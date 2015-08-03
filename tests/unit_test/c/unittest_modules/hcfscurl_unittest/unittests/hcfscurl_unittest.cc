extern "C" {
#include "hcfscurl.h"
}
#include "mock_params.h"
#include <string.h>
#include <gtest/gtest.h>


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
TEST(hcfs_init_swift_backendTest, InitBackendGetAuthFail)
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
		S3_BUCKET = "test_bucket";
		S3_SECRET = "test_key";
		S3_ACCESS = "test";

		write_list_header_flag = FALSE;
		write_auth_header_flag = FALSE;
	}

	void TearDown()
	{
		if (curl_handle)
			free(curl_handle);
	}

};

TEST_F(hcfs_list_containerTest, ListSwift_HttpPerformFail)
{
	CURRENT_BACKEND = SWIFT;
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
	strcpy(curl_handle->id, "_test_");
	
	EXPECT_EQ(-1, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftlisthead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/swiftlistbody_test_.tmp", F_OK));
}

TEST_F(hcfs_list_containerTest, ListSwiftSuccess)
{
	CURRENT_BACKEND = SWIFT;
	strcpy(curl_handle->id, "_test_");
	write_list_header_flag = TRUE;
	
	EXPECT_EQ(200, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/swiftlisthead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/swiftlistbody_test_.tmp", F_OK));

	write_list_header_flag = FALSE;	
}

TEST_F(hcfs_list_containerTest, ListS3_HttpPerformFail)
{
	CURRENT_BACKEND = S3;
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
	strcpy(curl_handle->id, "_test_");
	
	EXPECT_EQ(-1, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/S3listhead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/S3listbody_test_.tmp", F_OK));
	
}

TEST_F(hcfs_list_containerTest, ListS3Success)
{
	CURRENT_BACKEND = S3;
	strcpy(curl_handle->id, "_test_");
	write_list_header_flag = TRUE;
	
	EXPECT_EQ(200, hcfs_list_container(curl_handle));
	EXPECT_EQ(-1, access("/run/shm/S3listhead_test_.tmp", F_OK));
	EXPECT_EQ(-1, access("/run/shm/S3listbody_test_.tmp", F_OK));
	write_list_header_flag = FALSE;
	
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

	void SetUp()
	{
		curl_handle = (CURL_HANDLE *)malloc(sizeof(CURL_HANDLE));
		strcpy(swift_url_string, "https://127.0.0.1/fake_url");
		strcpy(swift_auth_string, 
			"X-Auth-Token: hello_swift_auth_string");
		SWIFT_CONTAINER = "test_container";
		S3_BUCKET = "test_bucket";
		S3_SECRET = "test_key";
		S3_ACCESS = "test";

		write_list_header_flag = FALSE;
		write_auth_header_flag = FALSE;
	}

	void TearDown()
	{
		if (curl_handle)
			free(curl_handle);
	}

};
/*
	End of unittest of hcfs_put_object()
 */
