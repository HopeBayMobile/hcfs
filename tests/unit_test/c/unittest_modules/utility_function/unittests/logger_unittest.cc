#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include <fcntl.h>
#include "logger.h"
#include "global.h"
#include "params.h"
#include "fuseop.h"
}
#include "gtest/gtest.h"

SYSTEM_CONF_STRUCT *system_config;

/* Begin of the test case for the function open_log */

class open_logTest : public ::testing::Test {
 protected:
  char tmpfilename[25];
  int32_t outfileno, errfileno;
  virtual void SetUp() {
    system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
    memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
    snprintf(tmpfilename, 25, "/tmp/testlog");
    logptr = NULL;
    outfileno = dup(fileno(stdout));
    errfileno = dup(fileno(stderr));
   }

  virtual void TearDown() {
    dup2(outfileno, fileno(stdout));
    dup2(errfileno, fileno(stderr));
    if (logptr != NULL) {
      if (logger_get_fileptr(logptr)) {
        fclose(logger_get_fileptr(logptr));
      }
      unlink(tmpfilename);
      sem_destroy(logger_get_semaphore(logptr));
      free(logptr);
    }
    free(system_config);
   }

 };

TEST_F(open_logTest, LogOpenOK) {
  int32_t ret;

  ret = open_log(tmpfilename);
  EXPECT_EQ(0, ret);
 }

TEST_F(open_logTest, LogDoubleOpenError) {
  int32_t ret;

  ret = open_log(tmpfilename);
  ASSERT_EQ(0, ret);
  ret = open_log((char *)"/tmp/testlog2");
  EXPECT_EQ(-EPERM, ret);
 }

TEST_F(open_logTest, LogFileOpenError) {
  int32_t ret;

  mknod(tmpfilename, S_IFREG | 0400, 0);
  ret = open_log(tmpfilename);
  EXPECT_EQ(-EACCES, ret);
  chmod(tmpfilename, 0600);
 }

/* End of the test case for the function open_log */

/* Begin of the test case for the function write_log */

class write_logTest : public ::testing::Test {
protected:
	char tmpfilename[25];
	int32_t outfileno, errfileno;
	virtual void SetUp() {
		system_config = (SYSTEM_CONF_STRUCT *)
				malloc(sizeof(SYSTEM_CONF_STRUCT));
		hcfs_system = (SYSTEM_DATA_HEAD *)
				malloc(sizeof(SYSTEM_DATA_HEAD));
		memset(hcfs_system, 0 ,sizeof(SYSTEM_DATA_HEAD));
		memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
		snprintf(tmpfilename, 25, "/tmp/testlog");
		logptr = NULL;
		outfileno = dup(fileno(stdout));
		errfileno = dup(fileno(stderr));
	}

	virtual void TearDown() {
		if (logptr != NULL) {
			if (logger_get_fileptr(logptr)) {
				fclose(logger_get_fileptr(logptr));
			}
			sem_destroy(logger_get_semaphore(logptr));
			free(logptr);
		}

		for (int i = 1; i <= NUM_LOG_FILE; i++) {
			char filename[100];
			sprintf(filename, "%s.%d", tmpfilename, i);
			if (!access(filename, F_OK))
				unlink(filename);
		}
		unlink(tmpfilename);
		free(system_config);
		free(hcfs_system);
	}

};

TEST_F(write_logTest, LogWriteOK) {
  int32_t ret;
  FILE *fptr;
  char tmpstr[100], tmpstr1[100], tmpstr2[100];

  ret = open_log(tmpfilename);
  ASSERT_EQ(0, ret);
  LOG_LEVEL = 10;
  write_log(10, "Thisisatest");

  close_log();

  dup2(outfileno, fileno(stdout));
  dup2(errfileno, fileno(stderr));

  fptr = fopen(tmpfilename, "r");
  ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
  fclose(fptr);
  ASSERT_EQ(3, ret);
  EXPECT_STREQ("Thisisatest", tmpstr2);
 }
TEST_F(write_logTest, NoLogEntry) {
  int32_t ret;
  FILE *fptr;
  char tmpstr[100], tmpstr1[100], tmpstr2[100];

  ret = open_log(tmpfilename);
  ASSERT_EQ(0, ret);
  LOG_LEVEL = 0;
  write_log(10, "Thisisatest");

  close_log();

  dup2(outfileno, fileno(stdout));
  dup2(errfileno, fileno(stderr));

  fptr = fopen(tmpfilename, "r");
  ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
  fclose(fptr);
  EXPECT_EQ(EOF, ret);
 }
TEST_F(write_logTest, NoLogWriteOK) {
  int32_t ret, failed;
  FILE *fptr;
  int32_t errcode;
  char tmpstr[100], tmpstr1[100], tmpstr2[100];

  fptr = fopen(tmpfilename, "a+");
  setbuf(fptr, NULL);
  if (fptr == NULL)
    failed = 1;
  else
    failed = 0;
  ASSERT_EQ(0, failed);

  errcode = 0;
  ret = dup2(fileno(fptr), fileno(stdout));
  errcode = errno;
  ASSERT_NE(-1, ret);
  setbuf(stdout, NULL);
  errcode = 0;
  ret = dup2(fileno(fptr), fileno(stderr));
  errcode = errno;
  ASSERT_NE(-1, ret);
  setbuf(stderr, NULL);
  LOG_LEVEL = 10;
  write_log(10, "Thisisatest");
  fclose(fptr);

  dup2(outfileno, fileno(stdout));
  dup2(errfileno, fileno(stderr));

  fptr = fopen(tmpfilename, "r");
  failed = 0;
  if (fptr == NULL)
    failed = 1;
  ASSERT_EQ(0, failed);
  ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
  fclose(fptr);
  ASSERT_EQ(3, ret);
  EXPECT_STREQ("Thisisatest", tmpstr2);
 }

TEST_F(write_logTest, LogShift_OK) {
	int32_t ret;
	FILE *fptr;
	char tmpstr[100], tmpstr1[100], tmpstr2[100];
	char log_file2[100];

	fptr = fopen(tmpfilename, "w+");
	ftruncate(fileno(fptr), MAX_LOG_FILE_SIZE);
	fclose(fptr);
	ret = open_log(tmpfilename);
	ASSERT_EQ(0, ret);
	LOG_LEVEL = 10;
	write_log(10, "Thisisatest");
	write_log(10, "Thisisatest2");

	close_log();

	dup2(outfileno, fileno(stdout));
	dup2(errfileno, fileno(stderr));

	/* Newer log file */
	fptr = fopen(tmpfilename, "r");
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest2", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(EOF, ret);
	fclose(fptr);

	/* Older log file */
	sprintf(log_file2, "%s.1", tmpfilename);
	fptr = fopen(log_file2, "r");
	ASSERT_TRUE(fptr != NULL);
	fseek(fptr, MAX_LOG_FILE_SIZE, SEEK_SET);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(EOF, ret);
	fclose(fptr);
	unlink(log_file2);
}

TEST_F(write_logTest, ManyLogFile_Shift_OK) {
	int32_t ret;
	FILE *fptr;
	char tmpstr[100], tmpstr1[100], tmpstr2[100];
	char log_file2[100];
	char log_msg[100];

	/* Mock log file */
	fptr = fopen(tmpfilename, "w+");
	ftruncate(fileno(fptr), MAX_LOG_FILE_SIZE);
	fclose(fptr);
	for (int i = 1; i <= NUM_LOG_FILE; i++) {
		sprintf(log_file2, "%s.%d", tmpfilename, i);
		fptr = fopen(log_file2, "w+");
		fseek(fptr, 0, SEEK_SET);
		sprintf(log_msg, "Thisisatest%d", i);
		fwrite(log_msg, strlen(log_msg), 1 ,fptr);
		fclose(fptr);
	}
	ret = open_log(tmpfilename);
	ASSERT_EQ(0, ret);
	LOG_LEVEL = 10;
	write_log(10, "Thisisatest");
	write_log(10, "Thisisatest2");

	close_log();

	dup2(outfileno, fileno(stdout));
	dup2(errfileno, fileno(stderr));

	/* Newer log file */
	fptr = fopen(tmpfilename, "r");
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest2", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(EOF, ret);
	fclose(fptr);

	sprintf(log_file2, "%s.1", tmpfilename);
	fptr = fopen(log_file2, "r");
	ASSERT_TRUE(fptr != NULL);
	fseek(fptr, MAX_LOG_FILE_SIZE, SEEK_SET);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(EOF, ret);
	fclose(fptr);
	unlink(log_file2);

	/* Older log file */
	for (int i = 2; i <= NUM_LOG_FILE; i++) {
		sprintf(log_file2, "%s.%d", tmpfilename, i);
		fptr = fopen(log_file2, "r");
		ASSERT_TRUE(fptr != NULL) << i;
		ret = fscanf(fptr, "%s", tmpstr2);
		ASSERT_EQ(1, ret);
		sprintf(log_msg, "Thisisatest%d", i - 1);
		EXPECT_STREQ(log_msg, tmpstr2);
		fclose(fptr);
		unlink(log_file2);
	}
}

TEST_F(write_logTest, LogMsgIsTooLong) {
	int32_t ret;
	FILE *fptr;
	char tmpstr[100], tmpstr1[100], tmpstr2[256];
	char longstr[256];

	strcpy(longstr, "ThisisatestThisisatestThisisatestThisisatest"
		"ThisisatestThisisatestThisisatestThisisatestThisisatest"
		"ThisisatestThisisatestThisisatestThisisatestThisisatest"
		"ThisisatestThisisatestThisisatestThisisatestThisisatest");
	ret = open_log(tmpfilename);
	ASSERT_EQ(0, ret);
	LOG_LEVEL = 10;
	write_log(10, "Thisisatest");
	write_log(10, "%s\n", longstr);
	write_log(10, "%s\n", longstr);
	write_log(10, "Thisisatest2");

	close_log();

	dup2(outfileno, fileno(stdout));
	dup2(errfileno, fileno(stderr));

	/* Check logs */
	fptr = fopen(tmpfilename, "r");
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ(longstr, tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ(longstr, tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest2", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(EOF, ret);
	fclose(fptr);
}


#ifdef LOG_COMPRESS TRUE
TEST_F(write_logTest, LogCompress_FlushOK) {
	int32_t ret;
	FILE *fptr;
	char tmpstr[100], tmpstr1[100], tmpstr2[100];

	ret = open_log(tmpfilename);
	ASSERT_EQ(0, ret);
	LOG_LEVEL = 5;
	for (int i = 0; i < 10; i++)
		write_log(5, "Thisisatest");

	sleep(FLUSH_TIME_INTERVAL + 1);
	//write_log(10, "Dummy log msg"); /* This log will not be recorded */

	close_log();

	dup2(outfileno, fileno(stdout));
	dup2(errfileno, fileno(stderr));

	fptr = fopen(tmpfilename, "r");
	rewind(fptr);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s [repeat 9 times]\n", tmpstr,
			tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(EOF, ret);
	fclose(fptr);
}

TEST_F(write_logTest, LogCompress_FlushOK2) {
	int32_t ret;
	FILE *fptr;
	char tmpstr[100], tmpstr1[100], tmpstr2[100];

	ret = open_log(tmpfilename);
	ASSERT_EQ(0, ret);
	LOG_LEVEL = 5;
	for (int i = 0; i < 10; i++)
		write_log(5, "Thisisatest");

	/* Do not need to sleep */
	write_log(5, "Thisisatest2");

	close_log();

	dup2(outfileno, fileno(stdout));
	dup2(errfileno, fileno(stderr));

	fptr = fopen(tmpfilename, "r");
	rewind(fptr);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s [repeat 9 times]\n", tmpstr,
			tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest2", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(EOF, ret);
	fclose(fptr);
}

TEST_F(write_logTest, LogCompress_OverDifferentLogFile) {
	int32_t ret;
	FILE *fptr;
	char tmpstr[100], tmpstr1[100], tmpstr2[100];
	char log_file2[100];

	fptr = fopen(tmpfilename, "w+");
	ftruncate(fileno(fptr), MAX_LOG_FILE_SIZE);
	fclose(fptr);
	ret = open_log(tmpfilename);
	ASSERT_EQ(0, ret);
	LOG_LEVEL = 10;
	write_log(10, "Thisisatest");
	write_log(10, "Thisisatest");
	write_log(10, "Thisisatest");
	write_log(10, "Thisisatest2");

	close_log();

	dup2(outfileno, fileno(stdout));
	dup2(errfileno, fileno(stderr));

	/* Newer log file */
	fptr = fopen(tmpfilename, "r");
	ret = fscanf(fptr, "%s %s\t%s [repeat 2 times]\n", tmpstr,
			tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest2", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(EOF, ret);
	fclose(fptr);

	/* Older log file */
	sprintf(log_file2, "%s.1", tmpfilename);
	fptr = fopen(log_file2, "r");
	ASSERT_TRUE(fptr != NULL);
	fseek(fptr, MAX_LOG_FILE_SIZE, SEEK_SET);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(3, ret);
	EXPECT_STREQ("Thisisatest", tmpstr2);
	ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
	ASSERT_EQ(EOF, ret);
	fclose(fptr);
	unlink(log_file2);
}
#endif

/* End of the test case for the function write_log */
