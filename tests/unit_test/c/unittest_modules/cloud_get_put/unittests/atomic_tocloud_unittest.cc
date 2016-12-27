#include <inttypes.h>
#include <dirent.h>
#include <ftw.h>
extern "C" {
#include "atomic_tocloud.h"
#include "fuseop.h"
#include "global.h"
#include "params.h"
#include "utils.h"
}
#include "gtest/gtest.h"
#include "mock_params.h"

#define RESPONSE_FAIL 0
#define RESPONSE_SUCCESS 1

extern SYSTEM_CONF_STRUCT *system_config;

static int do_delete (const char *fpath, const struct stat *sb,
		int32_t tflag, struct FTW *ftwbuf)
{
	switch (tflag) {
	case FTW_D:
	case FTW_DNR:
	case FTW_DP:
		rmdir (fpath);
		break;
	default:
		unlink (fpath);
		break;
	}
	return (0);
}

/*
 * Unittest for comm2fuseproc()
 */ 
class comm2fuseprocTest : public ::testing::Test {
protected:
	virtual void SetUp()
	{
		if (access(FUSE_SOCK_PATH, F_OK) == 0)
			unlink(FUSE_SOCK_PATH);
	}

	virtual void TearDown()
	{
		if (access(FUSE_SOCK_PATH, F_OK) == 0)
			unlink(FUSE_SOCK_PATH);
	}
};

void *mock_sock_connector(void *data)
{
	int socket_fd;
	int ac_fd;
	int socket_flag;
	struct sockaddr_un sock_addr;
	UPLOADING_COMMUNICATION_DATA comm_data;
	int resp;

	sock_addr.sun_family = AF_UNIX;
	strcpy(sock_addr.sun_path, FUSE_SOCK_PATH);
	socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);

	bind(socket_fd, (struct sockaddr *)&sock_addr,
			sizeof(struct sockaddr_un));

	//socket_flag = fcntl(socket_fd, F_GETFL, 0);
	//socket_flag |= O_NONBLOCK;
	//fcntl(socket_fd, F_SETFL, socket_flag);

	listen(socket_fd, 1);
	ac_fd = accept(socket_fd, NULL, NULL);

	recv(ac_fd, &comm_data, sizeof(UPLOADING_COMMUNICATION_DATA), 0);
	if (comm_data.progress_list_fd == RESPONSE_FAIL)
		resp = -1;
	if (comm_data.progress_list_fd == RESPONSE_SUCCESS)
		resp = 0;

	send(ac_fd, &resp, sizeof(int), 0);

	close(ac_fd);
	close(socket_fd);

	return NULL;
}

#ifndef _ANDROID_ENV_
TEST_F(comm2fuseprocTest, FailToConnect_SocketPathNotExist)
{
	int ret;
	int fd = 1;
	char status = TRUE;
	ino_t inode = 1;

	ret = 0;
	ret = comm2fuseproc(inode, status, fd, FALSE, FALSE);

	EXPECT_EQ(-ENOENT, ret);
}

TEST_F(comm2fuseprocTest, SucceedToConn_ResponseFail)
{
	int ret;
	pthread_t tid;
	int fd = RESPONSE_FAIL;
	char status = TRUE;
	ino_t inode = 1;

	pthread_create(&tid, NULL, mock_sock_connector, NULL);

	usleep(100000); /* Wait for connector */
	ret = 0;
	ret = comm2fuseproc(inode, status, fd, FALSE, FALSE);

	EXPECT_EQ(-1, ret);

	pthread_join(tid, NULL);
}

TEST_F(comm2fuseprocTest, SucceedToConn_ResponseSuccess)
{
	int ret;
	pthread_t tid;
	int fd = RESPONSE_SUCCESS;
	char status = TRUE;
	ino_t inode = 1;

	pthread_create(&tid, NULL, mock_sock_connector, NULL);

	usleep(100000); /* Wait for connector */
	ret = 0;
	ret = comm2fuseproc(inode, status, fd, FALSE, FALSE);

	EXPECT_EQ(0, ret);

	pthread_join(tid, NULL);
}
#else
TEST_F(comm2fuseprocTest, SucceedToConn_ResponseSuccess)
{
	int ret;
	pthread_t tid;
	int fd = RESPONSE_SUCCESS;
	char status = TRUE;
	ino_t inode = 1;

	ret = 0;
	ret = comm2fuseproc(inode, status, fd, FALSE, FALSE);

	EXPECT_EQ(0, ret);
}
#endif

/*
 * End of unittest for comm2fuseproc()
 */ 

/*
 * Unittest for init_progress_info()
 */ 
class init_progress_infoTest : public ::testing::Test {
protected:
	char mock_progress_path[100];
	uint8_t last_pin_status = 0;

	void SetUp()
	{
		strcpy(mock_progress_path, "/tmp/mock_progress_file");
		if (access(mock_progress_path, F_OK) == 0)
			unlink(mock_progress_path);
		if (access("/tmp/file_meta_init_progress_info", F_OK) == 0)
			unlink("/tmp/file_meta_init_progress_info");
	}

	void TearDown()
	{
		if (access(mock_progress_path, F_OK) == 0)
			unlink(mock_progress_path);
		if (access("/tmp/file_meta_init_progress_info", F_OK) == 0)
			unlink("/tmp/file_meta_init_progress_info");
	}
};

TEST_F(init_progress_infoTest, Init_Bad_fd)
{
	int fd;
	int ret;

	fd = 0;
	ret = init_progress_info(fd, 0, 0, NULL, &last_pin_status);

	EXPECT_EQ(-ESPIPE, ret);
}

TEST_F(init_progress_infoTest, Init_backend_fptr_Is_NULL)
{
	int fd;
	int ret;
	int64_t size;
	PROGRESS_META progress_meta;

	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);

	/* run */
	ret = init_progress_info(fd, 0, 0, NULL, &last_pin_status);

	/* verify */
	ASSERT_EQ(0, ret);
	pread(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	EXPECT_EQ(NOW_UPLOADING, progress_meta.now_action);
	EXPECT_EQ(0, progress_meta.backend_size);
	EXPECT_EQ(0, progress_meta.total_backend_blocks);
	size = lseek(fd, 0, SEEK_END);
	EXPECT_EQ(sizeof(PROGRESS_META), size);

	/* recycle */
	close(fd);
}

TEST_F(init_progress_infoTest, Init_BackendData_Success_All_TODELETE_NONE)
{
	int fd;
	int ret;
	int64_t size;
	PROGRESS_META progress_meta;
	FILE_META_TYPE tmp_file_meta;
	BLOCK_ENTRY_PAGE tmp_entry_page;
	FILE *file_metafptr;
	int num_pages;
	HCFS_STAT tmp_stat;

	/* Prepare mock metadata that all blocks are TODELETE and NONE */
	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);
	file_metafptr = fopen("/tmp/file_meta_init_progress_info", "w+");
	ASSERT_TRUE(NULL != file_metafptr);
	setbuf(file_metafptr, NULL);
	memset(&tmp_entry_page, 0, sizeof(BLOCK_ENTRY_PAGE));
	for (int i = 0; i < MAX_BLOCK_ENTRIES_PER_PAGE; i++) {
		tmp_entry_page.block_entries[i].status =
			(i % 2 ? ST_TODELETE : ST_NONE);
	}
	num_pages = 1000;
	memset(&tmp_stat, 0, sizeof(HCFS_STAT));
	memset(&tmp_file_meta, 0, sizeof(FILE_META_TYPE));

	fseek(file_metafptr, 0, SEEK_SET);
	fwrite(&tmp_stat, 1, sizeof(HCFS_STAT), file_metafptr);
	fwrite(&tmp_file_meta, 1, sizeof(FILE_META_TYPE), file_metafptr);
	for (int i = 0; i < num_pages ; i++) { // Linearly mock metadata
		fwrite(&tmp_entry_page, sizeof(BLOCK_ENTRY_PAGE), 1,
			file_metafptr);
	}

	/* run */
	ret = init_progress_info(fd, num_pages * MAX_BLOCK_ENTRIES_PER_PAGE,
		0, file_metafptr, &last_pin_status);

	/* verify */
	ASSERT_EQ(0, ret);
	pread(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	EXPECT_EQ(NOW_UPLOADING, progress_meta.now_action);
	EXPECT_EQ(0, progress_meta.backend_size);
	EXPECT_EQ(num_pages * MAX_BLOCK_ENTRIES_PER_PAGE,
		progress_meta.total_backend_blocks);
	EXPECT_EQ(0, progress_meta.direct);
	EXPECT_EQ(0, progress_meta.single_indirect);
	size = lseek(fd, 0, SEEK_END);
	EXPECT_EQ(sizeof(PROGRESS_META), size);

	/* recycle */
	close(fd);
	fclose(file_metafptr);
}

TEST_F(init_progress_infoTest, Init_BackendData_Success_All_BOTH_CLOUD_LDISK)
{
	int fd;
	int ret;
	int64_t size;
	PROGRESS_META progress_meta;
	FILE_META_TYPE tmp_file_meta;
	BLOCK_ENTRY_PAGE tmp_entry_page;
	FILE *file_metafptr;
	int num_pages;
	HCFS_STAT tmp_stat;

	/* Prepare mock metadata that all blocks are TODELETE and NONE */
	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);
	memset(&progress_meta, 0, sizeof(PROGRESS_META));
	pwrite(fd, &progress_meta, sizeof(PROGRESS_META), 0);

	file_metafptr = fopen("/tmp/file_meta_init_progress_info", "w+");
	ASSERT_TRUE(NULL != file_metafptr);
	setbuf(file_metafptr, NULL);
	memset(&tmp_entry_page, 0, sizeof(BLOCK_ENTRY_PAGE));
	for (int i = 0; i < MAX_BLOCK_ENTRIES_PER_PAGE; i++) {
		tmp_entry_page.block_entries[i].status =
			(i % 2 ? ST_CLOUD : ST_LDISK);
	}
	num_pages = 1 + POINTERS_PER_PAGE + POINTERS_PER_PAGE / 5;
	memset(&tmp_stat, 0, sizeof(HCFS_STAT));
	memset(&tmp_file_meta, 0, sizeof(FILE_META_TYPE));

	fseek(file_metafptr, 0, SEEK_SET);
	fwrite(&tmp_stat, 1, sizeof(HCFS_STAT), file_metafptr);
	fwrite(&tmp_file_meta, 1, sizeof(FILE_META_TYPE), file_metafptr);
	for (int i = 0; i < num_pages ; i++) { // Linearly mock metadata
		fwrite(&tmp_entry_page, 1, sizeof(BLOCK_ENTRY_PAGE),
			file_metafptr);
	}

	/* run */
	ret = init_progress_info(fd, num_pages * MAX_BLOCK_ENTRIES_PER_PAGE,
		123, file_metafptr, &last_pin_status);

	/* verify */
	ASSERT_EQ(0, ret);
	pread(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	EXPECT_EQ(NOW_UPLOADING, progress_meta.now_action);
	EXPECT_EQ(123, progress_meta.backend_size);
	EXPECT_EQ(num_pages * MAX_BLOCK_ENTRIES_PER_PAGE,
		progress_meta.total_backend_blocks);
	EXPECT_EQ(sizeof(PROGRESS_META), progress_meta.direct);
	EXPECT_EQ(sizeof(PROGRESS_META) + sizeof(BLOCK_UPLOADING_PAGE),
		progress_meta.single_indirect);
	EXPECT_EQ(sizeof(PROGRESS_META) + sizeof(BLOCK_UPLOADING_PAGE) +
		sizeof(PTR_ENTRY_PAGE) + sizeof(BLOCK_UPLOADING_PAGE) *
		POINTERS_PER_PAGE, progress_meta.double_indirect);
	EXPECT_EQ(0, progress_meta.triple_indirect);
	EXPECT_EQ(0, progress_meta.quadruple_indirect);
	for (int i = 0; i < num_pages * MAX_BLOCK_ENTRIES_PER_PAGE ; i++) {
		BLOCK_UPLOADING_STATUS tmp_block_status;
		int ret;
		ret = get_progress_info(fd, i, &tmp_block_status);
		EXPECT_EQ(ret, 0);
		EXPECT_EQ(TRUE,
			CLOUD_BLOCK_EXIST(tmp_block_status.block_exist));
		EXPECT_EQ(FALSE,
			TOUPLOAD_BLOCK_EXIST(tmp_block_status.block_exist));
	}
	/* recycle */
	close(fd);
	fclose(file_metafptr);
}

TEST_F(init_progress_infoTest, Init_BackendData_Success_NONE_EndWith_LDISK)
{
	int fd;
	int ret;
	int64_t size;
	PROGRESS_META progress_meta;
	FILE_META_TYPE tmp_file_meta;
	BLOCK_ENTRY_PAGE tmp_entry_page;
	FILE *file_metafptr;
	int num_pages;
	HCFS_STAT tmp_stat;

	/* Prepare mock metadata that all blocks are TODELETE and NONE */
	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);
	memset(&progress_meta, 0, sizeof(PROGRESS_META));
	pwrite(fd, &progress_meta, sizeof(PROGRESS_META), 0);

	file_metafptr = fopen("/tmp/file_meta_init_progress_info", "w+");
	ASSERT_TRUE(NULL != file_metafptr);
	setbuf(file_metafptr, NULL);
	memset(&tmp_entry_page, 0, sizeof(BLOCK_ENTRY_PAGE));
	tmp_entry_page.block_entries[MAX_BLOCK_ENTRIES_PER_PAGE - 1].status =
		ST_LDISK; // Last element is LDISK
	num_pages = 1 + POINTERS_PER_PAGE + POINTERS_PER_PAGE / 5;
	memset(&tmp_stat, 0, sizeof(HCFS_STAT));
	memset(&tmp_file_meta, 0, sizeof(FILE_META_TYPE));

	fseek(file_metafptr, 0, SEEK_SET);
	fwrite(&tmp_stat, 1, sizeof(HCFS_STAT), file_metafptr);
	fwrite(&tmp_file_meta, 1, sizeof(FILE_META_TYPE), file_metafptr);
	for (int i = 0; i < num_pages ; i++) { // Linearly mock metadata
		fwrite(&tmp_entry_page, 1, sizeof(BLOCK_ENTRY_PAGE),
			file_metafptr);
	}

	/* run */
	ret = init_progress_info(fd, num_pages * MAX_BLOCK_ENTRIES_PER_PAGE,
		123, file_metafptr, &last_pin_status);

	/* verify */
	ASSERT_EQ(0, ret);
	pread(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	EXPECT_EQ(NOW_UPLOADING, progress_meta.now_action);
	EXPECT_EQ(123, progress_meta.backend_size);
	EXPECT_EQ(num_pages * MAX_BLOCK_ENTRIES_PER_PAGE,
		progress_meta.total_backend_blocks);
	EXPECT_EQ(sizeof(PROGRESS_META), progress_meta.direct);
	EXPECT_EQ(sizeof(PROGRESS_META) + sizeof(BLOCK_UPLOADING_PAGE),
		progress_meta.single_indirect);
	EXPECT_EQ(sizeof(PROGRESS_META) + sizeof(BLOCK_UPLOADING_PAGE) +
		sizeof(PTR_ENTRY_PAGE) + sizeof(BLOCK_UPLOADING_PAGE) *
		POINTERS_PER_PAGE, progress_meta.double_indirect);
	EXPECT_EQ(0, progress_meta.triple_indirect);
	EXPECT_EQ(0, progress_meta.quadruple_indirect);
	for (int i = 0; i < num_pages * MAX_BLOCK_ENTRIES_PER_PAGE ; i++) {
		int ret;
		BLOCK_UPLOADING_STATUS tmp_block_status;
		ret = get_progress_info(fd, i, &tmp_block_status);
		EXPECT_EQ(ret, 0);
		if ((i + 1) % MAX_BLOCK_ENTRIES_PER_PAGE == 0) {
			EXPECT_EQ(TRUE,
				CLOUD_BLOCK_EXIST(tmp_block_status.block_exist));
			EXPECT_EQ(FALSE,
				TOUPLOAD_BLOCK_EXIST(tmp_block_status.block_exist));
		} else {
			EXPECT_EQ(FALSE,
				CLOUD_BLOCK_EXIST(tmp_block_status.block_exist));
			EXPECT_EQ(FALSE,
				TOUPLOAD_BLOCK_EXIST(tmp_block_status.block_exist));
		}
	}

	/* recycle */
	close(fd);
	fclose(file_metafptr);
}
/*
 * End of unittest for init_progress_info()
 */ 

/*
 * Unittest for set_progress_info()
 */ 
class set_progress_infoTest : public ::testing::Test {
protected:
	char mock_progress_path[100];

	void SetUp()
	{
		strcpy(mock_progress_path, "/tmp/mock_progress_file");
		if (access(mock_progress_path, F_OK) == 0)
			unlink(mock_progress_path);
	}

	void TearDown()
	{
		if (access(mock_progress_path, F_OK) == 0)
			unlink(mock_progress_path);
	}
};

TEST_F(set_progress_infoTest, SetProgressSuccess)
{
	int fd;
	int ret;
	ssize_t tmp_size;
	int64_t num_blocks;
	BLOCK_UPLOADING_STATUS ans_status, empty_status;
	PROGRESS_META tmp_meta;

	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);
	tmp_size = lseek(fd, 0, SEEK_END);
	ASSERT_EQ(0, tmp_size);
	num_blocks = 1200 * MAX_BLOCK_ENTRIES_PER_PAGE;

#if ENABLE(DEDUP)
	unsigned char toupload_objid[OBJID_LENGTH], backend_objid[OBJID_LENGTH];

	memset(toupload_objid, 'K', OBJID_LENGTH);
	memset(backend_objid, 'W', OBJID_LENGTH);
#else
	int64_t toupload_seq, backend_seq;

	toupload_seq = 123;
	backend_seq = 456;
#endif
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);

	/* Run tested function. Set progress info for even blocks */
	for (int i = 0; i < num_blocks; i += 2) {
		char toupload_exist = TRUE;
		char backend_exist = TRUE;
		char finish = TRUE;
		int ret = -1;

#if ENABLE(DEDUP)
		ret = set_progress_info(fd, i, &toupload_exist,
			&backend_exist, toupload_objid, backend_objid, &finish);
#else
		ret = set_progress_info(fd, i, &toupload_exist,
			&backend_exist, &toupload_seq, &backend_seq, &finish);
#endif
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	memset(&empty_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
	memset(&ans_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
#if ENABLE(DEDUP)
	memcpy(ans_status.to_upload_objid, toupload_objid, OBJID_LENGTH);
	memcpy(ans_status.backend_objid, backend_objid, OBJID_LENGTH);
#else
	ans_status.to_upload_seq = toupload_seq;
	ans_status.backend_seq = backend_seq;
#endif
	ans_status.finish_uploading = TRUE;
	SET_TOUPLOAD_BLOCK_EXIST(ans_status.block_exist);
	SET_CLOUD_BLOCK_EXIST(ans_status.block_exist);

	for(int i = 0; i < num_blocks; i++) {
		BLOCK_UPLOADING_STATUS block_status, *tmp_status;

		ret = get_progress_info(fd, i, &block_status);
		ASSERT_EQ(0, ret);

		if (i % 2 == 0)
			ASSERT_EQ(0, memcmp(&ans_status, &block_status,
				sizeof(BLOCK_UPLOADING_STATUS))) << "i = " << i;
		else
			ASSERT_EQ(0, memcmp(&empty_status, &block_status,
				sizeof(BLOCK_UPLOADING_STATUS))) << "i = " << i;
	}

	/* Recycle */
	close(fd);
}

TEST_F(set_progress_infoTest, SetProgressSuccess_ManyDifferentBlockLevel)
{
	int fd;
	int ret;
	ssize_t tmp_size;
	int64_t block_index[5];
	BLOCK_UPLOADING_STATUS ans_status, empty_status;
	PROGRESS_META tmp_meta;

	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);
	tmp_size = lseek(fd, 0, SEEK_END);
	ASSERT_EQ(0, tmp_size);

	block_index[0] = MAX_BLOCK_ENTRIES_PER_PAGE / 2;
	block_index[1] = MAX_BLOCK_ENTRIES_PER_PAGE +
		(POINTERS_PER_PAGE / 2) * MAX_BLOCK_ENTRIES_PER_PAGE;
	block_index[2] = MAX_BLOCK_ENTRIES_PER_PAGE + 
		POINTERS_PER_PAGE * MAX_BLOCK_ENTRIES_PER_PAGE +
		(POINTERS_PER_PAGE / 2) * POINTERS_PER_PAGE * 
		MAX_BLOCK_ENTRIES_PER_PAGE;
	block_index[3] = MAX_BLOCK_ENTRIES_PER_PAGE + 
		POINTERS_PER_PAGE * MAX_BLOCK_ENTRIES_PER_PAGE +
		(long long)POINTERS_PER_PAGE * POINTERS_PER_PAGE * 
		MAX_BLOCK_ENTRIES_PER_PAGE +
		(long long)(POINTERS_PER_PAGE / 2) * POINTERS_PER_PAGE *
		POINTERS_PER_PAGE * MAX_BLOCK_ENTRIES_PER_PAGE;
	block_index[4] = MAX_BLOCK_ENTRIES_PER_PAGE + 
		POINTERS_PER_PAGE * MAX_BLOCK_ENTRIES_PER_PAGE +
		(long long)POINTERS_PER_PAGE * POINTERS_PER_PAGE * MAX_BLOCK_ENTRIES_PER_PAGE +
		(long long)POINTERS_PER_PAGE * POINTERS_PER_PAGE * POINTERS_PER_PAGE * 
		MAX_BLOCK_ENTRIES_PER_PAGE + MAX_BLOCK_ENTRIES_PER_PAGE / 2;
	

#if ENABLE(DEDUP)
	unsigned char toupload_objid[OBJID_LENGTH], backend_objid[OBJID_LENGTH];

	memset(toupload_objid, 'K', OBJID_LENGTH);
	memset(backend_objid, 'W', OBJID_LENGTH);
#else
	int64_t toupload_seq, backend_seq;

	toupload_seq = 123;
	backend_seq = 456;
#endif
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);


	/* Run tested function. Set progress info for even blocks */
	for (int i = 0; i < 5; i ++) {
		char toupload_exist = TRUE;
		char backend_exist = TRUE;
		char finish = TRUE;
		int ret = -1;

#if ENABLE(DEDUP)
		ret = set_progress_info(fd, block_index[i], &toupload_exist,
			&backend_exist, toupload_objid, backend_objid, &finish);
#else
		ret = set_progress_info(fd, block_index[i], &toupload_exist,
			&backend_exist, &toupload_seq, &backend_seq, &finish);
#endif
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	memset(&empty_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
	memset(&ans_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
#if ENABLE(DEDUP)
	memcpy(ans_status.to_upload_objid, toupload_objid, OBJID_LENGTH);
	memcpy(ans_status.backend_objid, backend_objid, OBJID_LENGTH);
#else
	ans_status.to_upload_seq = toupload_seq;
	ans_status.backend_seq = backend_seq;
#endif
	ans_status.finish_uploading = TRUE;
	SET_TOUPLOAD_BLOCK_EXIST(ans_status.block_exist);
	SET_CLOUD_BLOCK_EXIST(ans_status.block_exist);

	for(int i = 0; i < 5; i++) {
		BLOCK_UPLOADING_STATUS block_status, *tmp_status;

		ret = get_progress_info(fd, block_index[i], &block_status);
		ASSERT_EQ(0, ret);

		ASSERT_EQ(0, memcmp(&ans_status, &block_status,
			sizeof(BLOCK_UPLOADING_STATUS))) << "i = " << i;
	}

	/* Recycle */
	close(fd);
}
/*
 * End of unittest for set_progress_info()
 */ 

/*
 * Unittest for get_progress_info()
 */ 
class get_progress_infoTest : public ::testing::Test {
protected:
	char mock_progress_path[100];

	void SetUp()
	{
		strcpy(mock_progress_path, "/tmp/mock_progress_file");
		if (access(mock_progress_path, F_OK) == 0)
			unlink(mock_progress_path);
	}

	void TearDown()
	{
		if (access(mock_progress_path, F_OK) == 0)
			unlink(mock_progress_path);
	}
};

TEST_F(get_progress_infoTest, GetEmptyProgressFileSuccess)
{
	int fd;
	ssize_t tmp_size;
	BLOCK_UPLOADING_STATUS tmp_block_status, empty_block_status;
	PROGRESS_META progress_meta;
	int num_blocks;
	int ret;

	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);
	tmp_size = lseek(fd, 0, SEEK_END);
	ASSERT_EQ(0, tmp_size);
	num_blocks = 1000 * MAX_BLOCK_ENTRIES_PER_PAGE;
	memset(&progress_meta, 0, sizeof(PROGRESS_META));
	pwrite(fd, &progress_meta, sizeof(PROGRESS_META), 0);

	memset(&empty_block_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
	/* Run get_progress_info() */
	for (int i = 0; i < num_blocks; i++) {
		memset(&tmp_block_status, 1, sizeof(BLOCK_UPLOADING_STATUS));
		ret = get_progress_info(fd, i, &tmp_block_status);
		ASSERT_EQ(-ENOENT, ret);
		ASSERT_EQ(FALSE, tmp_block_status.finish_uploading);
		ASSERT_EQ(0, memcmp(&empty_block_status, &tmp_block_status,
			sizeof(BLOCK_UPLOADING_STATUS)));
	}

	/* Recycle */
	close(fd);
}
/*
 * End of unittest for get_progress_info()
 */ 

/*
 * Unittest for create_progress_file()
 */ 
class create_progress_fileTest : public ::testing::Test {
protected:
	char upload_bullpen_path[200];
	char path[200];

	void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
		METAPATH = "/tmp";
		sprintf(upload_bullpen_path, "%s/upload_bullpen", METAPATH);

		if (!access(upload_bullpen_path, F_OK))
			nftw(upload_bullpen_path, do_delete, 20, FTW_DEPTH);
	}

	void TearDown()
	{
		if (!access(path, F_OK))
			unlink(path);

		if (!access(upload_bullpen_path, F_OK))
			nftw(upload_bullpen_path, do_delete, 20, FTW_DEPTH);

		free(system_config);
	}
};

TEST_F(create_progress_fileTest, upload_pullpen_NotExist_OpenSuccess)
{
	int fd;
	int inode;

	inode = 3;
	fd = create_progress_file(inode);

	/* Verify */
	sprintf(path, "%s/upload_progress_inode_%d",
		upload_bullpen_path, inode);
	EXPECT_GT(fd, 0);
	EXPECT_EQ(0, access(path, F_OK));

	/* Recycle */
	close(fd);
	unlink(path);
	nftw(upload_bullpen_path, do_delete, 20, FTW_DEPTH);
}

TEST_F(create_progress_fileTest, progressfile_Exist_ReOpenSuccess)
{
	int fd;
	int inode;
	struct stat new_stat, old_stat;

	inode = 3;
	sprintf(path, "%s/upload_progress_inode_%d",
		upload_bullpen_path, inode);
	mkdir(upload_bullpen_path, 0700);
	mknod(path, 0700, 0);
	stat(path, &old_stat);

	fd = create_progress_file(inode);

	/* Verify */
	stat(upload_bullpen_path, &new_stat);
	EXPECT_GT(fd, 0);
	EXPECT_EQ(0, access(path, F_OK));
	EXPECT_NE(old_stat.st_ino, new_stat.st_ino);

	/* Recycle */
	close(fd);
	unlink(path);
	nftw(upload_bullpen_path, do_delete, 20, FTW_DEPTH);
}
/*
 * End of unittest for create_progress_file()
 */ 

/*
 * Unittest for del_progress_file()
 */ 
class del_progress_fileTest : public ::testing::Test {
protected:
	char upload_bullpen_path[200];
	char path[200];

	void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
		METAPATH = "/tmp";
		sprintf(upload_bullpen_path, "%s/upload_bullpen", METAPATH);

		if (!access(upload_bullpen_path, F_OK))
			nftw(upload_bullpen_path, do_delete, 20, FTW_DEPTH);
	}

	void TearDown()
	{
		if (!access(path, F_OK))
			unlink(path);

		if (!access(upload_bullpen_path, F_OK))
			nftw(upload_bullpen_path, do_delete, 20, FTW_DEPTH);

		free(system_config);
	}
};

TEST_F(del_progress_fileTest, CloseSuccess)
{
	int fd;
	int inode;
	int ret;

	inode = 3;
	sprintf(path, "%s/upload_progress_inode_%d",
		upload_bullpen_path, inode);
	mkdir(upload_bullpen_path, 0700);
	fd = open(path, O_CREAT | O_RDWR);

	/* Run */
	ret = del_progress_file(fd, inode);

	/* Verify */
	EXPECT_EQ(0, ret);
	EXPECT_EQ(0, access(upload_bullpen_path, F_OK));
	EXPECT_EQ(-1, access(path, F_OK));

	nftw(upload_bullpen_path, do_delete, 20, FTW_DEPTH);
}
/*
 * End of unittest for del_progress_file()
 */ 

/*
 * Unittest for check_and_copy_file()
 */ 
class check_and_copy_fileTest : public ::testing::Test {
protected:
	char *mock_source, *mock_target;

	virtual void SetUp()
	{
		mock_target = "/tmp/copy_target";

		system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
		hcfs_system = (SYSTEM_DATA_HEAD *)
				malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0, sizeof(SYSTEM_DATA_HEAD));
		if (!access(mock_target, F_OK))
			unlink(mock_target);
	}

	virtual void TearDown()
	{
		mock_target = "/tmp/copy_target";

		if (!access(mock_target, F_OK))
			unlink(mock_target);
		free(hcfs_system);
		free(system_config);
	}
};

TEST_F(check_and_copy_fileTest, SourceFileNotExist)
{
	int ret;

	mock_source = "/tmp/hahaha";
	mock_target = "/tmp/copy_target";

	ret = check_and_copy_file(mock_source, mock_target, TRUE, FALSE);

	EXPECT_EQ(-ENOENT, ret);
	EXPECT_EQ(-1, access(mock_source, F_OK));
	EXPECT_EQ(0, access(mock_target, F_OK));
	EXPECT_EQ(0, hcfs_system->systemdata.system_size);
	EXPECT_EQ(0, hcfs_system->systemdata.cache_size);
}

TEST_F(check_and_copy_fileTest, TargetFileExist_CopySuccess)
{
	int ret;
	FILE *src, *tar;
	char src_buf[200], tar_buf[200];
	int64_t filesize;
	struct stat tmpstat;

	mock_source = "unittests/atomic_tocloud_unittest.cc";
	mock_target = "/tmp/copy_target";
	stat(mock_source, &tmpstat);
	filesize = tmpstat.st_size;

	mknod(mock_target, 0700, 0);
	ret = check_and_copy_file(mock_source, mock_target, TRUE, FALSE);

	EXPECT_EQ(0, ret);
	EXPECT_EQ(0, access(mock_source, F_OK));
	EXPECT_EQ(0, access(mock_target, F_OK));
	src = fopen(mock_source, "r");
	tar = fopen(mock_target, "r");
	ASSERT_TRUE(src != NULL);
	ASSERT_TRUE(tar != NULL);
	while (fgets(src_buf, 150, src)) {
		fgets(tar_buf, 150, tar);
		ASSERT_EQ(0, strcmp(src_buf, tar_buf));
	}
	EXPECT_EQ(0, hcfs_system->systemdata.system_size);
	EXPECT_EQ(round_size(filesize), hcfs_system->systemdata.cache_size);

	/* Recycle */
	fclose(src);
	fclose(tar);
	unlink(mock_target);
}

TEST_F(check_and_copy_fileTest, CopyFail_NoSpace)
{
	int ret;
	struct stat tmpstat;

	mock_source = "unittests/atomic_tocloud_unittest.cc";
	mock_target = "/tmp/copy_target";

	CACHE_HARD_LIMIT = 123456;
	hcfs_system->systemdata.cache_size = CACHE_HARD_LIMIT + 1;
	ret = check_and_copy_file(mock_source, mock_target, TRUE, TRUE);

	EXPECT_EQ(-ENOSPC, ret);
	EXPECT_EQ(0, access(mock_source, F_OK));
	EXPECT_EQ(0, access(mock_target, F_OK));
	stat(mock_target, &tmpstat);
	EXPECT_EQ(0, tmpstat.st_size);
	EXPECT_EQ(0, hcfs_system->systemdata.system_size);
	EXPECT_EQ(CACHE_HARD_LIMIT + 1, hcfs_system->systemdata.cache_size);

	/* Recycle */
	unlink(mock_target);
}

TEST_F(check_and_copy_fileTest, CopySuccess)
{
	int ret;
	FILE *src, *tar;
	char src_buf[200], tar_buf[200];

	mock_source = "unittests/atomic_tocloud_unittest.cc";
	mock_target = "/tmp/copy_target";

	ret = check_and_copy_file(mock_source, mock_target, TRUE, FALSE);

	EXPECT_EQ(0, ret);
	EXPECT_EQ(0, access(mock_source, F_OK));
	EXPECT_EQ(0, access(mock_target, F_OK));
	src = fopen(mock_source, "r");
	tar = fopen(mock_target, "r");
	ASSERT_TRUE(src != NULL);
	ASSERT_TRUE(tar != NULL);
	while (fgets(src_buf, 150, src)) {
		fgets(tar_buf, 150, tar);
		ASSERT_EQ(0, strcmp(src_buf, tar_buf));
	}

	/* Recycle */
	fclose(src);
	fclose(tar);
	unlink(mock_target);
}
/*
 * End of unittest for check_and_copy_file()
 */ 

/* 
 * Unittest for continue_inode_sync()
 */
class continue_inode_syncTest : public ::testing::Test {
protected:
	char bullpen_path[200];
	char progress_path[200];
	char toupload_metapath[200];
	char backend_metapath[200];
	char localmetapath[200];

	virtual void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
		METAPATH = "/tmp";
		sprintf(bullpen_path, "%s/upload_bullpen", METAPATH);
		if (!access(bullpen_path, F_OK))
			nftw(bullpen_path, do_delete, 20, FTW_DEPTH);
		init_sync_control();
		memset(&test_sync_struct, 0, sizeof(TEST_REVERT_STRUCT));
		sem_init(&test_sync_struct.record_sem, 0, 1);
		memset(&test_delete_struct, 0, sizeof(TEST_REVERT_STRUCT));
		sem_init(&test_delete_struct.record_sem, 0, 1);
		mkdir(bullpen_path, 0700);

		fetch_meta_path(localmetapath, 0);
		mknod(localmetapath, 0700, 0);
	}

	virtual void TearDown()
	{
		if (!access(progress_path, F_OK))
			unlink(progress_path);
		if (!access(toupload_metapath, F_OK))
			unlink(toupload_metapath);
		if (!access(backend_metapath, F_OK))
			unlink(backend_metapath);
		if (!access(bullpen_path, F_OK))
			nftw(bullpen_path, do_delete, 20, FTW_DEPTH);
		sem_destroy(&(sync_ctl.sync_op_sem));
		sem_destroy(&(sync_ctl.sync_queue_sem));
		nftw(bullpen_path, do_delete, 20, FTW_DEPTH);
		free(system_config);

		unlink(localmetapath);
	}

	void init_sync_control(void)
	{
		memset(&sync_ctl, 0, sizeof(SYNC_THREAD_CONTROL));
		sem_init(&(sync_ctl.sync_op_sem), 0, 1);
		sem_init(&(sync_ctl.sync_queue_sem), 0, MAX_SYNC_CONCURRENCY);
		memset(&(sync_ctl.threads_in_use), 0,
				sizeof(ino_t) * MAX_SYNC_CONCURRENCY);
		memset(&(sync_ctl.threads_created), 0,
				sizeof(char) * MAX_SYNC_CONCURRENCY);
		sync_ctl.total_active_sync_threads = 0;

	}

};


TEST_F(continue_inode_syncTest, Revert_NonRegfile)
{
	int ret;
	int fd;
	int inode;
	PROGRESS_META tmp_meta;
	SYNC_THREAD_TYPE sync_type;

	/* Prepare mock data */
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.now_action = NOW_UPLOADING; // Finish init
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);

	fetch_toupload_meta_path(toupload_metapath, inode);
	mknod(toupload_metapath, 0700, 0); // make a toupload_meta
	fetch_backend_meta_path(backend_metapath, inode);
	mknod(backend_metapath, 0700, 0); // make a backend_meta
	sync_type.this_mode = S_IFDIR;
	sync_type.inode = inode;
	sync_type.progress_fd = fd;
	sync_type.which_index = 0;
	sync_ctl.threads_error[0] = FALSE;
	sync_ctl.threads_finished[0] = FALSE;

	/* Run */
	continue_inode_sync(&sync_type);

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(0, access(progress_path, F_OK));
	EXPECT_EQ(TRUE, sync_ctl.threads_error[0]);
	EXPECT_EQ(TRUE, sync_ctl.threads_finished[0]);
	unlink(progress_path);
}


TEST_F(continue_inode_syncTest, Crash_AfterOpenProgressFile)
{
	int ret;
	int fd;
	int inode;
	pthread_t terminate_tid;
	PROGRESS_META tmp_meta;
	SYNC_THREAD_TYPE sync_type;

	/* Prepare mock data */
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.now_action = PREPARING; // Finish init
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);
	sync_type.this_mode = S_IFREG;
	sync_type.inode = inode;
	sync_type.progress_fd = fd;
	sync_type.which_index = 0;
	sync_ctl.threads_error[0] = FALSE;
	sync_ctl.threads_finished[0] = FALSE;

	/* Run */
	continue_inode_sync(&sync_type);
	close(fd);

	/* Verify */
	EXPECT_EQ(0, access(progress_path, F_OK));
	EXPECT_EQ(TRUE, sync_ctl.threads_error[0]);
	EXPECT_EQ(TRUE, sync_ctl.threads_finished[0]);
	unlink(progress_path);
}

TEST_F(continue_inode_syncTest, Crash_AfterCopyLocalMeta)
{
	int ret;
	int fd;
	int inode;
	pthread_t terminate_tid;
	SYNC_THREAD_TYPE sync_type;
	PROGRESS_META tmp_meta;

	/* Prepare mock data */
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.now_action = PREPARING; // Finish init
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);

	fetch_toupload_meta_path(toupload_metapath, inode);
	mknod(toupload_metapath, 0700, 0); // make a toupload_meta
	sync_type.this_mode = S_IFREG;
	sync_type.inode = inode;
	sync_type.progress_fd = fd;
	sync_type.which_index = 0;
	sync_ctl.threads_error[0] = FALSE;
	sync_ctl.threads_finished[0] = FALSE;

	/* Run */
	continue_inode_sync(&sync_type);
	close(fd);

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(0, access(progress_path, F_OK));
	EXPECT_EQ(TRUE, sync_ctl.threads_error[0]);
	EXPECT_EQ(TRUE, sync_ctl.threads_finished[0]);
	unlink(progress_path);
}

TEST_F(continue_inode_syncTest, Crash_AfterDownloadBackendMeta_BeforeFinishInit)
{
	int ret;
	int fd;
	int inode;
	SYNC_THREAD_TYPE sync_type;
	PROGRESS_META tmp_meta;

	/* Prepare mock data */
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.now_action = PREPARING; // Finish init
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);
	
	fetch_toupload_meta_path(toupload_metapath, inode);
	mknod(toupload_metapath, 0700, 0); // make a toupload_meta
	fetch_backend_meta_path(backend_metapath, inode);
	mknod(backend_metapath, 0700, 0); // make a backend_meta
	sync_type.this_mode = S_IFREG;
	sync_type.inode = inode;
	sync_type.progress_fd = fd;
	sync_type.which_index = 0;
	sync_ctl.threads_error[0] = FALSE;
	sync_ctl.threads_finished[0] = FALSE;

	/* Run */
	continue_inode_sync(&sync_type);
	close(fd);

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(0, access(progress_path, F_OK));
	EXPECT_EQ(TRUE, sync_ctl.threads_error[0]);
	EXPECT_EQ(TRUE, sync_ctl.threads_finished[0]);
	unlink(progress_path);
}

TEST_F(continue_inode_syncTest, Crash_AfterFinishInit_ProgressFile)
{
	int ret;
	int fd;
	int inode;
	PROGRESS_META tmp_meta;
	SYNC_THREAD_TYPE sync_type;

	/* Prepare mock data */
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.now_action = NOW_UPLOADING; // Finish init
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);

	fetch_toupload_meta_path(toupload_metapath, inode);
	mknod(toupload_metapath, 0700, 0); // make a toupload_meta
	sync_type.this_mode = S_IFREG;
	sync_type.inode = inode;
	sync_type.progress_fd = fd;
	sync_type.which_index = 0;
	sync_ctl.threads_error[0] = FALSE;
	sync_ctl.threads_finished[0] = FALSE;

	/* Run */
	continue_inode_sync(&sync_type);

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(0, access(progress_path, F_OK));
	unlink(progress_path);	
}

TEST_F(continue_inode_syncTest, Crash_AfterUnlinkBackendmeta_KeepOnUploading)
{
	int ret;
	int fd;
	int inode;
	PROGRESS_META tmp_meta;
	SYNC_THREAD_TYPE sync_type;

	/* Prepare mock data */
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.now_action = NOW_UPLOADING; // Finish init
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);
	
	fetch_toupload_meta_path(toupload_metapath, inode);
	mknod(toupload_metapath, 0700, 0); // make a toupload_meta
	sync_type.this_mode = S_IFREG;
	sync_type.inode = inode;
	sync_type.progress_fd = fd;
	sync_type.which_index = 0;
	sync_ctl.threads_error[0] = FALSE;
	sync_ctl.threads_finished[0] = FALSE;

	/* Run */
	continue_inode_sync(&sync_type);
	close(fd);

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(0, access(progress_path, F_OK));
	EXPECT_EQ(1, test_sync_struct.total_inode);
	EXPECT_EQ(inode, test_sync_struct.record_uploading_inode[0]);

	unlink(toupload_metapath);
	unlink(progress_path);	
}

TEST_F(continue_inode_syncTest, Crash_When_DeleteOldData_KeepDeleting)
{
	int ret;
	int fd;
	int inode;
	PROGRESS_META tmp_meta;
	pthread_t terminate_tid;
	SYNC_THREAD_TYPE sync_type;

	/* Prepare mock data */
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.now_action = DEL_BACKEND_BLOCKS; // Finish init
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);
	
	fetch_toupload_meta_path(toupload_metapath, inode);
	fetch_backend_meta_path(backend_metapath, inode);
	sync_type.this_mode = S_IFREG;
	sync_type.inode = inode;
	sync_type.progress_fd = fd;
	sync_type.which_index = 0;
	sync_ctl.threads_error[0] = FALSE;
	sync_ctl.threads_finished[0] = FALSE;

	/* Run */
	continue_inode_sync(&sync_type);
	close(fd);

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(0, access(progress_path, F_OK));
	EXPECT_EQ(1, test_delete_struct.total_inode);
	EXPECT_EQ(inode, test_delete_struct.record_uploading_inode[0]);
	EXPECT_EQ(TRUE, sync_ctl.threads_error[0]);
	EXPECT_EQ(TRUE, sync_ctl.threads_finished[0]);

	unlink(toupload_metapath);
	unlink(progress_path);
}
/* 
 * Unittest for continue_inode_sync()
 */

/*
 * Unittest for init_backend_file_info()
 */
class init_backend_file_infoTest : public ::testing::Test {
protected:
	char backend_metapath[200];
	char bullpen_path[200];
	char progress_path[200];
	int fd;
	uint8_t last_pin_status = 0;

	void SetUp()
	{
		system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
		METAPATH = "/tmp";
		MAX_BLOCK_SIZE = 1048576;
		sprintf(bullpen_path, "%s/upload_bullpen", METAPATH);
		cleanup(bullpen_path);
		if (!access(bullpen_path, F_OK))
			nftw(bullpen_path, do_delete, 20, FTW_DEPTH);
		mkdir(bullpen_path, 0700);
	}

	void TearDown()
	{
		cleanup(bullpen_path);
		if (!access(backend_metapath, F_OK))
			unlink(backend_metapath);
		free(system_config);
		nftw(bullpen_path, do_delete, 20, FTW_DEPTH);
	}
	void cleanup(char *path) {
	  DIR *dirp;
	  dirent *tmpentry;
	  char tmpname[400];

	  dirp = opendir(path);
	  if (dirp == NULL)
	    return;
	  while ((tmpentry = readdir(dirp)) != NULL) {
	    if (!strcmp(tmpentry->d_name, "."))
	      continue;
	    if (!strcmp(tmpentry->d_name, ".."))
	      continue;
	    snprintf(tmpname, 400, "%s/%s", path, tmpentry->d_name);
	    unlink(tmpname);
	   }
	  closedir(dirp);
	 }
};

TEST_F(init_backend_file_infoTest, NotRevert_FirstUpload)
{
	int64_t backend_size, total_backend_blocks;
	SYNC_THREAD_TYPE sync_type;
	PROGRESS_META tmp_meta;
	int ret;
	int inode;

	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);

	sync_type.this_mode = S_IFREG;
	sync_type.is_revert = FALSE;
	sync_type.progress_fd = fd;
	sync_type.inode = inode;
	is_first_upload = TRUE;
	fetch_from_cloud_fail = FALSE;

	/* Run */
	ret = init_backend_file_info(&sync_type, &backend_size,
			&total_backend_blocks, 0, &last_pin_status);

	/* Verify */
	fetch_backend_meta_path(backend_metapath, inode);
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(0, ret);
	EXPECT_EQ(0, backend_size);
	EXPECT_EQ(0, total_backend_blocks);
	close(fd);
	unlink(progress_path);
}

TEST_F(init_backend_file_infoTest, NotRevert_FailToFetchFromCloud)
{
	int64_t backend_size, total_backend_blocks;
	SYNC_THREAD_TYPE sync_type;
	PROGRESS_META tmp_meta;
	int ret;
	int inode;

	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);

	sync_type.this_mode = S_IFREG;
	sync_type.is_revert = FALSE;
	sync_type.progress_fd = fd;
	sync_type.inode = inode;
	is_first_upload = TRUE;
	fetch_from_cloud_fail = TRUE;

	/* Run */
	ret = init_backend_file_info(&sync_type, &backend_size,
			&total_backend_blocks, 1, &last_pin_status);

	/* Verify */
	fetch_backend_meta_path(backend_metapath, inode);
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(-EIO, ret);
	close(fd);
	unlink(progress_path);
}

TEST_F(init_backend_file_infoTest, NotRevert_NotFirstUpload)
{
	int64_t backend_size, total_backend_blocks;
	SYNC_THREAD_TYPE sync_type;
	PROGRESS_META tmp_meta;
	int ret;
	int inode;

	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);

	sync_type.this_mode = S_IFREG;
	sync_type.is_revert = FALSE;
	sync_type.progress_fd = fd;
	sync_type.inode = inode;
	is_first_upload = FALSE;
	fetch_from_cloud_fail = FALSE;

	/* Run */
	ret = init_backend_file_info(&sync_type, &backend_size,
			&total_backend_blocks, 1, &last_pin_status);

	/* Verify */
	fetch_backend_meta_path(backend_metapath, inode);
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(0, ret);
	EXPECT_EQ(MAX_BLOCK_SIZE * 10, backend_size);
	EXPECT_EQ(10, total_backend_blocks);
	close(fd);
	unlink(progress_path);
}

TEST_F(init_backend_file_infoTest, RevertMode_FinishInit)
{
	int64_t backend_size, total_backend_blocks;
	SYNC_THREAD_TYPE sync_type;
	PROGRESS_META tmp_meta;
	int ret;
	int inode;

	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.now_action = NOW_UPLOADING;
	tmp_meta.backend_size = MAX_BLOCK_SIZE * 10;
	tmp_meta.total_backend_blocks = 10;
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);

	sync_type.this_mode = S_IFREG;
	sync_type.is_revert = TRUE;
	sync_type.progress_fd = fd;
	sync_type.inode = inode;
	is_first_upload = FALSE;
	fetch_from_cloud_fail = FALSE;

	/* Run */
	ret = init_backend_file_info(&sync_type, &backend_size,
			&total_backend_blocks, 1, &last_pin_status);

	/* Verify */
	fetch_backend_meta_path(backend_metapath, inode);
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(0, ret);
	EXPECT_EQ(MAX_BLOCK_SIZE * 10, backend_size);
	EXPECT_EQ(10, total_backend_blocks);
	close(fd);
	unlink(progress_path);
}

TEST_F(init_backend_file_infoTest, RevertMode_NotFinishInit)
{
	int64_t backend_size, total_backend_blocks;
	SYNC_THREAD_TYPE sync_type;
	PROGRESS_META tmp_meta;
	int32_t ret;
	int inode;

	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.now_action = PREPARING;
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);

	sync_type.this_mode = S_IFREG;
	sync_type.is_revert = TRUE;
	sync_type.progress_fd = fd;
	sync_type.inode = inode;
	is_first_upload = FALSE;
	fetch_from_cloud_fail = FALSE;

	/* Run */
	ret = init_backend_file_info(&sync_type, &backend_size,
			&total_backend_blocks, 1, &last_pin_status);

	/* Verify */
	fetch_backend_meta_path(backend_metapath, inode);
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(ENOENT, errno);
	EXPECT_EQ(-ECANCELED, ret);
	close(fd);
	unlink(progress_path);
}

/*
 * End of unittest for init_backend_file_info()
 */
