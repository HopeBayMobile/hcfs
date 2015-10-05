extern "C" {
#include "atomic_tocloud.h"
#include "fuseop.h"
#include "global.h"
#include "params.h"
}
#include "gtest/gtest.h"
#include "mock_params.h"

#define RESPONSE_FAIL 0
#define RESPONSE_SUCCESS 1

SYSTEM_CONF_STRUCT system_config;

class tag_status_on_fuseTest : public ::testing::Test {
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

TEST_F(tag_status_on_fuseTest, FailToConnect_SocketPathNotExist)
{
	int ret;
	int fd = 1;
	char status = TRUE;
	ino_t inode = 1;

	ret = 0;
	ret = tag_status_on_fuse(inode, status, fd);

	EXPECT_EQ(-ENOENT, ret);
}

TEST_F(tag_status_on_fuseTest, SucceedToConn_ResponseFail)
{
	int ret;
	pthread_t tid;
	int fd = RESPONSE_FAIL;
	char status = TRUE;
	ino_t inode = 1;

	pthread_create(&tid, NULL, mock_sock_connector, NULL);

	usleep(100000); /* Wait for connector */
	ret = 0;
	ret = tag_status_on_fuse(inode, status, fd);

	EXPECT_EQ(-1, ret);

	pthread_join(tid, NULL);
}

TEST_F(tag_status_on_fuseTest, SucceedToConn_ResponseSuccess)
{
	int ret;
	pthread_t tid;
	int fd = RESPONSE_SUCCESS;
	char status = TRUE;
	ino_t inode = 1;

	pthread_create(&tid, NULL, mock_sock_connector, NULL);

	usleep(100000); /* Wait for connector */
	ret = 0;
	ret = tag_status_on_fuse(inode, status, fd);

	EXPECT_EQ(0, ret);

	pthread_join(tid, NULL);
}

class init_progress_infoTest : public ::testing::Test {
protected:
	char mock_progress_path[100];

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
	ret = init_progress_info(fd, 0, 0, NULL);

	EXPECT_EQ(-ESPIPE, ret);
}

TEST_F(init_progress_infoTest, Init_backend_fptr_Is_NULL)
{
	int fd;
	int ret;
	long long size;
	PROGRESS_META progress_meta;

	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);

	/* run */
	ret = init_progress_info(fd, 0, 0, NULL);

	/* verify */
	ASSERT_EQ(0, ret);
	pread(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	EXPECT_EQ(TRUE, progress_meta.finish_init_backend_data);
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
	long long size;
	PROGRESS_META progress_meta;
	FILE_META_TYPE tmp_file_meta;
	BLOCK_ENTRY_PAGE tmp_entry_page;
	FILE *file_metafptr;
	int num_pages;
	struct stat tmp_stat;

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
	memset(&tmp_stat, 0, sizeof(struct stat));
	memset(&tmp_file_meta, 0, sizeof(FILE_META_TYPE));

	fseek(file_metafptr, 0, SEEK_SET);
	fwrite(&tmp_stat, 1, sizeof(struct stat), file_metafptr);
	fwrite(&tmp_file_meta, 1, sizeof(FILE_META_TYPE), file_metafptr);
	for (int i = 0; i < num_pages ; i++) { // Linearly mock metadata
		fwrite(&tmp_entry_page, sizeof(BLOCK_ENTRY_PAGE), 1,
			file_metafptr);
	}

	/* run */
	ret = init_progress_info(fd, num_pages * MAX_BLOCK_ENTRIES_PER_PAGE,
		0, file_metafptr);

	/* verify */
	ASSERT_EQ(0, ret);
	pread(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	EXPECT_EQ(TRUE, progress_meta.finish_init_backend_data);
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
	long long size;
	PROGRESS_META progress_meta;
	FILE_META_TYPE tmp_file_meta;
	BLOCK_ENTRY_PAGE tmp_entry_page;
	FILE *file_metafptr;
	int num_pages;
	struct stat tmp_stat;

	/* Prepare mock metadata that all blocks are TODELETE and NONE */
	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);
	file_metafptr = fopen("/tmp/file_meta_init_progress_info", "w+");
	ASSERT_TRUE(NULL != file_metafptr);
	setbuf(file_metafptr, NULL);
	memset(&tmp_entry_page, 0, sizeof(BLOCK_ENTRY_PAGE));
	for (int i = 0; i < MAX_BLOCK_ENTRIES_PER_PAGE; i++) {
		tmp_entry_page.block_entries[i].status =
			(i % 2 ? ST_CLOUD : ST_LDISK);
	}
	num_pages = 1 + POINTERS_PER_PAGE + POINTERS_PER_PAGE / 5;
	memset(&tmp_stat, 0, sizeof(struct stat));
	memset(&tmp_file_meta, 0, sizeof(FILE_META_TYPE));

	fseek(file_metafptr, 0, SEEK_SET);
	fwrite(&tmp_stat, 1, sizeof(struct stat), file_metafptr);
	fwrite(&tmp_file_meta, 1, sizeof(FILE_META_TYPE), file_metafptr);
	for (int i = 0; i < num_pages ; i++) { // Linearly mock metadata
		fwrite(&tmp_entry_page, 1, sizeof(BLOCK_ENTRY_PAGE),
			file_metafptr);
	}

	/* run */
	ret = init_progress_info(fd, num_pages * MAX_BLOCK_ENTRIES_PER_PAGE,
		123, file_metafptr);

	/* verify */
	ASSERT_EQ(0, ret);
	pread(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	EXPECT_EQ(TRUE, progress_meta.finish_init_backend_data);
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
		EXPECT_GT(ret, 0);
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
	long long size;
	PROGRESS_META progress_meta;
	FILE_META_TYPE tmp_file_meta;
	BLOCK_ENTRY_PAGE tmp_entry_page;
	FILE *file_metafptr;
	int num_pages;
	struct stat tmp_stat;

	/* Prepare mock metadata that all blocks are TODELETE and NONE */
	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);
	file_metafptr = fopen("/tmp/file_meta_init_progress_info", "w+");
	ASSERT_TRUE(NULL != file_metafptr);
	setbuf(file_metafptr, NULL);
	memset(&tmp_entry_page, 0, sizeof(BLOCK_ENTRY_PAGE));
	tmp_entry_page.block_entries[MAX_BLOCK_ENTRIES_PER_PAGE - 1].status =
		ST_LDISK; // Last element is LDISK
	num_pages = 1 + POINTERS_PER_PAGE + POINTERS_PER_PAGE / 5;
	memset(&tmp_stat, 0, sizeof(struct stat));
	memset(&tmp_file_meta, 0, sizeof(FILE_META_TYPE));

	fseek(file_metafptr, 0, SEEK_SET);
	fwrite(&tmp_stat, 1, sizeof(struct stat), file_metafptr);
	fwrite(&tmp_file_meta, 1, sizeof(FILE_META_TYPE), file_metafptr);
	for (int i = 0; i < num_pages ; i++) { // Linearly mock metadata
		fwrite(&tmp_entry_page, 1, sizeof(BLOCK_ENTRY_PAGE),
			file_metafptr);
	}

	/* run */
	ret = init_progress_info(fd, num_pages * MAX_BLOCK_ENTRIES_PER_PAGE,
		123, file_metafptr);

	/* verify */
	ASSERT_EQ(0, ret);
	pread(fd, &progress_meta, sizeof(PROGRESS_META), 0);
	EXPECT_EQ(TRUE, progress_meta.finish_init_backend_data);
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
		EXPECT_GT(ret, 0);
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
	long long num_blocks;
	BLOCK_UPLOADING_STATUS ans_status, empty_status;
	PROGRESS_META tmp_meta;

	fd = open(mock_progress_path, O_CREAT | O_RDWR);
	ASSERT_GT(fd, 0);
	tmp_size = lseek(fd, 0, SEEK_END);
	ASSERT_EQ(0, tmp_size);
	num_blocks = 1200 * MAX_BLOCK_ENTRIES_PER_PAGE;

#if (DEDUP_ENABLE)
	unsigned char toupload_objid[OBJID_LENGTH], backend_objid[OBJID_LENGTH];

	memset(toupload_objid, 'K', OBJID_LENGTH);
	memset(backend_objid, 'W', OBJID_LENGTH);
#else
	long long toupload_seq, backend_seq;

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

#if (DEDUP_ENABLE)
		ret = set_progress_info(fd, i, &toupload_exist,
			&backend_exist, toupload_objid, backend_objid, &finish);
#else
#endif
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	memset(&empty_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
	memset(&ans_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
#if (DEDUP_ENABLE)
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
		ASSERT_EQ(sizeof(BLOCK_UPLOADING_PAGE), ret);

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
	long long block_index[5];
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
	

#if (DEDUP_ENABLE)
	unsigned char toupload_objid[OBJID_LENGTH], backend_objid[OBJID_LENGTH];

	memset(toupload_objid, 'K', OBJID_LENGTH);
	memset(backend_objid, 'W', OBJID_LENGTH);
#else
	long long toupload_seq, backend_seq;

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

#if (DEDUP_ENABLE)
		ret = set_progress_info(fd, block_index[i], &toupload_exist,
			&backend_exist, toupload_objid, backend_objid, &finish);
#else
#endif
		ASSERT_EQ(0, ret);
	}

	/* Verify */
	memset(&empty_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
	memset(&ans_status, 0, sizeof(BLOCK_UPLOADING_STATUS));
#if (DEDUP_ENABLE)
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
		ASSERT_EQ(sizeof(BLOCK_UPLOADING_PAGE), ret);

		ASSERT_EQ(0, memcmp(&ans_status, &block_status,
			sizeof(BLOCK_UPLOADING_STATUS))) << "i = " << i;
	}

	/* Recycle */
	close(fd);
}

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
		ret = -1;
		ret = get_progress_info(fd, i, &tmp_block_status);
		ASSERT_EQ(0, ret);
		ASSERT_EQ(FALSE, tmp_block_status.finish_uploading);
		ASSERT_EQ(0, memcmp(&empty_block_status, &tmp_block_status,
			sizeof(BLOCK_UPLOADING_STATUS)));
	}

	/* Recycle */
	close(fd);
}

class open_progress_infoTest : public ::testing::Test {
protected:
	char upload_bullpen_path[200];
	char path[200];

	void SetUp()
	{
		METAPATH = "/tmp";
		sprintf(upload_bullpen_path, "%s/upload_bullpen", METAPATH);

		if (!access(upload_bullpen_path, F_OK))
			rmdir(upload_bullpen_path);
	}

	void TearDown()
	{
		if (!access(path, F_OK))
			unlink(path);

		if (!access(upload_bullpen_path, F_OK))
			rmdir(upload_bullpen_path);
	}
};

TEST_F(open_progress_infoTest, upload_pullpen_NotExist_OpenSuccess)
{
	int fd;
	int inode;

	inode = 3;
	fd = open_progress_info(inode);

	/* Verify */
	sprintf(path, "%s/upload_progress_inode_%d",
		upload_bullpen_path, inode);
	EXPECT_GT(fd, 0);
	EXPECT_EQ(0, access(path, F_OK));

	/* Recycle */
	close(fd);
	unlink(path);
	rmdir(upload_bullpen_path);
}

TEST_F(open_progress_infoTest, progressfile_Exist_ReOpenSuccess)
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

	fd = open_progress_info(inode);

	/* Verify */
	stat(upload_bullpen_path, &new_stat);
	EXPECT_GT(fd, 0);
	EXPECT_EQ(0, access(path, F_OK));
	EXPECT_NE(old_stat.st_ino, new_stat.st_ino);

	/* Recycle */
	close(fd);
	unlink(path);
	rmdir(upload_bullpen_path);
}

class close_progress_infoTest : public ::testing::Test {
protected:
	char upload_bullpen_path[200];
	char path[200];

	void SetUp()
	{
		METAPATH = "/tmp";
		sprintf(upload_bullpen_path, "%s/upload_bullpen", METAPATH);

		if (!access(upload_bullpen_path, F_OK))
			rmdir(upload_bullpen_path);
	}

	void TearDown()
	{
		if (!access(path, F_OK))
			unlink(path);

		if (!access(upload_bullpen_path, F_OK))
			rmdir(upload_bullpen_path);
	}
};

TEST_F(close_progress_infoTest, CloseSuccess)
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
	ret = close_progress_info(fd, inode);

	/* Verify */
	EXPECT_EQ(0, ret);
	EXPECT_EQ(0, access(upload_bullpen_path, F_OK));
	EXPECT_EQ(-1, access(path, F_OK));

	rmdir(upload_bullpen_path);
}

class check_and_copy_fileTest : public ::testing::Test {
protected:
	char *mock_source, *mock_target;

	virtual void SetUp()
	{
		if (!access(mock_target, F_OK))
			unlink(mock_target);
	}

	virtual void TearDown()
	{
		if (!access(mock_target, F_OK))
			unlink(mock_target);
	}
};

TEST_F(check_and_copy_fileTest, SourceFileNotExist)
{
	int ret;

	mock_source = "/tmp/hahaha";
	mock_target = "/tmp/copy_target";

	ret = check_and_copy_file(mock_source, mock_target);

	EXPECT_EQ(-ENOENT, ret);
	EXPECT_EQ(-1, access(mock_source, F_OK));
	EXPECT_EQ(-1, access(mock_target, F_OK));
}

TEST_F(check_and_copy_fileTest, TargetFileExist)
{
	int ret;

	mock_source = "unittests/atomic_tocloud_unittest.cc";
	mock_target = "/tmp/copy_target";

	mknod(mock_target, 0700, 0);
	ret = check_and_copy_file(mock_source, mock_target);

	EXPECT_EQ(-EEXIST, ret);
	EXPECT_EQ(0, access(mock_source, F_OK));
	EXPECT_EQ(0, access(mock_target, F_OK));

	unlink(mock_target);
}

TEST_F(check_and_copy_fileTest, CopySuccess)
{
	int ret;
	FILE *src, *tar;
	char src_buf[200], tar_buf[200];

	mock_source = "unittests/atomic_tocloud_unittest.cc";
	mock_target = "/tmp/copy_target";

	ret = check_and_copy_file(mock_source, mock_target);

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

class uploading_revertTest : public ::testing::Test {
public:
	static char keep_on;

protected:
	char bullpen_path[200];
	char progress_path[200];
	char toupload_metapath[200];
	char backend_metapath[200];

	virtual void SetUp()
	{
		METAPATH = "/tmp";
		sprintf(bullpen_path, "%s/upload_bullpen", METAPATH);
		if (!access(bullpen_path, F_OK))
			rmdir(bullpen_path);
		init_sync_control();
		memset(&test_sync_struct, 0, sizeof(TEST_REVERT_STRUCT));
		sem_init(&test_sync_struct.record_sem, 0, 1);
		memset(&test_delete_struct, 0, sizeof(TEST_REVERT_STRUCT));
		sem_init(&test_delete_struct.record_sem, 0, 1);
	}

	virtual void TearDown()
	{
		if (!access(progress_path, F_OK))
			unlink(progress_path);
		if (!access(toupload_metapath, F_OK))
			unlink(toupload_metapath);
		if (!access(bullpen_path, F_OK))
			rmdir(bullpen_path);
		sem_destroy(&(sync_ctl.sync_op_sem));
		sem_destroy(&(sync_ctl.sync_queue_sem));
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

char uploading_revertTest::keep_on = TRUE;
void *terminate_sync_threads(void *data)
{
	int ret;

	while (uploading_revertTest::keep_on) {
		for (int count = 0; count < MAX_SYNC_CONCURRENCY;
				count++) {
			if (sync_ctl.threads_in_use[count] != 0) {
				ret = pthread_tryjoin_np(
						sync_ctl.inode_sync_thread[count],
						NULL);
				if (ret == 0) {
					close_progress_info(
						sync_ctl.progress_fd[count],
						sync_ctl.threads_in_use[count]);
					sem_wait(&sync_ctl.sync_op_sem);
					sync_ctl.threads_in_use[count] = 0;
					sync_ctl.threads_created[count] = FALSE;
					sync_ctl.total_active_sync_threads--;
					sem_post(&sync_ctl.sync_queue_sem);
					sem_post(&sync_ctl.sync_op_sem);
				}
			}
		}
	}
}

TEST_F(uploading_revertTest, BullpenNotExist)
{
	int ret;

	ret = uploading_revert();
	EXPECT_EQ(0, ret);
}

TEST_F(uploading_revertTest, BullpenCannotBeOpened)
{
	int ret;

	mkdir(bullpen_path, 0000);

	ret = uploading_revert();
	EXPECT_EQ(-EACCES, ret);

	rmdir(bullpen_path);
}

TEST_F(uploading_revertTest, Crash_AfterOpenProgressFile)
{
	int ret;
	int fd;
	int inode;
	pthread_t terminate_tid;

	/* Prepare mock data */
	mkdir(bullpen_path, 0700);
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	close(fd);

	keep_on = TRUE;
	pthread_create(&terminate_tid, NULL,
			terminate_sync_threads, NULL);
	/* Run */
	ret = uploading_revert();
	EXPECT_EQ(0, ret);
	keep_on = FALSE;
	pthread_join(terminate_tid, NULL);	

	/* Verify */
	EXPECT_EQ(-1, access(progress_path, F_OK));

	/* Recycle */
	rmdir(bullpen_path);
}

TEST_F(uploading_revertTest, Crash_AfterCopyLocalMeta)
{
	int ret;
	int fd;
	int inode;
	pthread_t terminate_tid;

	/* Prepare mock data */
	mkdir(bullpen_path, 0700);
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	close(fd);
	
	fetch_toupload_meta_path(toupload_metapath, inode);
	mknod(toupload_metapath, 0700, 0); // make a toupload_meta

	keep_on = TRUE;
	pthread_create(&terminate_tid, NULL,
			terminate_sync_threads, NULL);
	/* Run */
	ret = uploading_revert();
	EXPECT_EQ(0, ret);
	keep_on = FALSE;
	pthread_join(terminate_tid, NULL);	

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(-1, access(progress_path, F_OK));

	/* Recycle */
	rmdir(bullpen_path);
}

TEST_F(uploading_revertTest, Crash_AfterDownloadBackendMeta)
{
	int ret;
	int fd;
	int inode;
	pthread_t terminate_tid;

	/* Prepare mock data */
	mkdir(bullpen_path, 0700);
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	close(fd);
	
	fetch_toupload_meta_path(toupload_metapath, inode);
	mknod(toupload_metapath, 0700, 0); // make a toupload_meta
	fetch_backend_meta_path(backend_metapath, inode);
	mknod(backend_metapath, 0700, 0); // make a backend_meta

	keep_on = TRUE;
	pthread_create(&terminate_tid, NULL,
			terminate_sync_threads, NULL);
	/* Run */
	ret = uploading_revert();
	EXPECT_EQ(0, ret);
	keep_on = FALSE;
	pthread_join(terminate_tid, NULL);	

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(-1, access(progress_path, F_OK));

	/* Recycle */
	rmdir(bullpen_path);
}

TEST_F(uploading_revertTest, Crash_BeforeFinishInit_ProgressFile)
{
	int ret;
	int fd;
	int inode;
	PROGRESS_META tmp_meta;
	pthread_t terminate_tid;

	/* Prepare mock data */
	mkdir(bullpen_path, 0700);
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);
	close(fd);
	
	fetch_toupload_meta_path(toupload_metapath, inode);
	mknod(toupload_metapath, 0700, 0); // make a toupload_meta
	fetch_backend_meta_path(backend_metapath, inode);
	mknod(backend_metapath, 0700, 0); // make a backend_meta

	keep_on = TRUE;
	pthread_create(&terminate_tid, NULL,
			terminate_sync_threads, NULL);
	/* Run */
	ret = uploading_revert();
	EXPECT_EQ(0, ret);
	keep_on = FALSE;
	pthread_join(terminate_tid, NULL);	

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(-1, access(progress_path, F_OK));

	/* Recycle */
	rmdir(bullpen_path);
}

TEST_F(uploading_revertTest, Crash_AfterFinishInit_ProgressFile)
{
	int ret;
	int fd;
	int inode;
	PROGRESS_META tmp_meta;
	pthread_t terminate_tid;

	/* Prepare mock data */
	mkdir(bullpen_path, 0700);
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.finish_init_backend_data = TRUE; // Finish init
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);
	close(fd);
	
	fetch_toupload_meta_path(toupload_metapath, inode);
	mknod(toupload_metapath, 0700, 0); // make a toupload_meta
	fetch_backend_meta_path(backend_metapath, inode);
	mknod(backend_metapath, 0700, 0); // make a backend_meta

	keep_on = TRUE;
	pthread_create(&terminate_tid, NULL,
			terminate_sync_threads, NULL);
	/* Run */
	ret = uploading_revert();
	EXPECT_EQ(0, ret);
	keep_on = FALSE;
	pthread_join(terminate_tid, NULL);	

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(-1, access(progress_path, F_OK));

	/* Recycle */
	rmdir(bullpen_path);
}

TEST_F(uploading_revertTest, Crash_AfterUnlinkBackendmeta_KeepOnUploading)
{
	int ret;
	int fd;
	int inode;
	PROGRESS_META tmp_meta;
	pthread_t terminate_tid;

	/* Prepare mock data */
	mkdir(bullpen_path, 0700);
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.finish_init_backend_data = TRUE; // Finish init
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);
	close(fd);
	
	fetch_toupload_meta_path(toupload_metapath, inode);
	mknod(toupload_metapath, 0700, 0); // make a toupload_meta
	fetch_backend_meta_path(backend_metapath, inode);

	keep_on = TRUE;
	pthread_create(&terminate_tid, NULL,
			terminate_sync_threads, NULL);
	/* Run */
	ret = uploading_revert();
	EXPECT_EQ(0, ret);
	keep_on = FALSE;
	pthread_join(terminate_tid, NULL);	

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(-1, access(progress_path, F_OK));
	EXPECT_EQ(1, test_sync_struct.total_inode);
	EXPECT_EQ(inode, test_sync_struct.record_uploading_inode[0]);

	/* Recycle */
	unlink(toupload_metapath);
	rmdir(bullpen_path);
}

TEST_F(uploading_revertTest, Crash_When_DeleteOldData_KeepDeleting)
{
	int ret;
	int fd;
	int inode;
	PROGRESS_META tmp_meta;
	pthread_t terminate_tid;

	/* Prepare mock data */
	mkdir(bullpen_path, 0700);
	inode = 3;
	sprintf(progress_path, "%s/upload_progress_inode_%d",
		bullpen_path, inode);
	fd = open(progress_path, O_CREAT | O_RDWR);
	memset(&tmp_meta, 0, sizeof(PROGRESS_META));
	tmp_meta.finish_init_backend_data = TRUE; // Finish init
	pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);
	close(fd);
	
	fetch_toupload_meta_path(toupload_metapath, inode);
	fetch_backend_meta_path(backend_metapath, inode);

	keep_on = TRUE;
	pthread_create(&terminate_tid, NULL,
			terminate_sync_threads, NULL);
	/* Run */
	ret = uploading_revert();
	EXPECT_EQ(0, ret);
	keep_on = FALSE;
	pthread_join(terminate_tid, NULL);	

	/* Verify */
	EXPECT_EQ(-1, access(toupload_metapath, F_OK));
	EXPECT_EQ(-1, access(backend_metapath, F_OK));
	EXPECT_EQ(-1, access(progress_path, F_OK));
	EXPECT_EQ(1, test_delete_struct.total_inode);
	EXPECT_EQ(inode, test_delete_struct.record_uploading_inode[0]);

	/* Recycle */
	rmdir(bullpen_path);
}

TEST_F(uploading_revertTest, Crash_AfterUnlinkBackendmeta_KeepOnUploading_ManyInode)
{
	int ret;
	int fd;
	int num_inode = 15; // Cannot exceed MAX_SYNC_CONCURRENCY
	int inode[num_inode];
	PROGRESS_META tmp_meta;
	pthread_t terminate_tid;

	/* Prepare mock data */
	mkdir(bullpen_path, 0700);
	
	for (int i = 1; i <= num_inode; i++) {
		sprintf(progress_path, "%s/upload_progress_inode_%d",
				bullpen_path, i);
		fd = open(progress_path, O_CREAT | O_RDWR);
		memset(&tmp_meta, 0, sizeof(PROGRESS_META));
		tmp_meta.finish_init_backend_data = TRUE; // Finish init
		pwrite(fd, &tmp_meta, sizeof(PROGRESS_META), 0);
		close(fd);

		fetch_toupload_meta_path(toupload_metapath, i);
		mknod(toupload_metapath, 0700, 0); // make a toupload_meta
	}

	keep_on = TRUE;
	pthread_create(&terminate_tid, NULL,
			terminate_sync_threads, NULL);
	/* Run */
	ret = uploading_revert();
	EXPECT_EQ(0, ret);
	keep_on = FALSE;
	pthread_join(terminate_tid, NULL);	

	/* Verify */
	for (int i = 1; i <= num_inode; i++) {	
		fetch_toupload_meta_path(toupload_metapath, i);
		EXPECT_EQ(-1, access(toupload_metapath, F_OK));

		fetch_backend_meta_path(backend_metapath, i);
		EXPECT_EQ(-1, access(backend_metapath, F_OK));

		sprintf(progress_path, "%s/upload_progress_inode_%d",
				bullpen_path, i);
		EXPECT_EQ(-1, access(progress_path, F_OK));
	}
	EXPECT_EQ(num_inode, test_sync_struct.total_inode);
	//EXPECT_EQ(inode, test_sync_struct.record_uploading_inode[0]);

	/* Recycle */
	unlink(toupload_metapath);
	rmdir(bullpen_path);
}

