extern "C" {
#include "atomic_tocloud.h"
#include "global.h"
}
#include "gtest/gtest.h"


#define RESPONSE_FAIL 0
#define RESPONSE_SUCCESS 1

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
	}

	void TearDown()
	{
		if (access(mock_progress_path, F_OK) == 0)
			unlink(mock_progress_path);
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

TEST_F(init_progress_infoTest, Init_BackendData_Success)
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
