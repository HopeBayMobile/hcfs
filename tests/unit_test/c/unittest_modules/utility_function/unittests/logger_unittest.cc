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
}
#include "gtest/gtest.h"

SYSTEM_CONF_STRUCT system_config;

/* Begin of the test case for the function open_log */

class open_logTest : public ::testing::Test {
 protected:
  char tmpfilename[25];
  int outfileno, errfileno;
  virtual void SetUp() {
    snprintf(tmpfilename, 25, "/tmp/testlog");
    logptr = NULL;
    outfileno = dup(fileno(stdout));
    errfileno = dup(fileno(stderr));
   }

  virtual void TearDown() {
    dup2(outfileno, fileno(stdout));
    dup2(errfileno, fileno(stderr));
    if (logptr != NULL) {
      if (logptr->fptr != NULL) {
        fclose(logptr->fptr);
      }
      unlink(tmpfilename);
      sem_destroy(&(logptr->logsem));
      free(logptr);
    }
   }

 };

TEST_F(open_logTest, LogOpenOK) {
  int ret;

  ret = open_log(tmpfilename);
  EXPECT_EQ(0, ret);
 }

TEST_F(open_logTest, LogDoubleOpenError) {
  int ret;

  ret = open_log(tmpfilename);
  ASSERT_EQ(0, ret);
  ret = open_log((char *)"/tmp/testlog2");
  EXPECT_EQ(-EPERM, ret);
 }

TEST_F(open_logTest, LogFileOpenError) {
  int ret;

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
  int outfileno, errfileno;
  virtual void SetUp() {
    snprintf(tmpfilename, 25, "/tmp/testlog");
    logptr = NULL;
    outfileno = dup(fileno(stdout));
    errfileno = dup(fileno(stderr));
   }

  virtual void TearDown() {
    dup2(outfileno, fileno(stdout));
    dup2(errfileno, fileno(stderr));
    if (logptr != NULL) {
      if (logptr->fptr != NULL) {
        fclose(logptr->fptr);
      }
      unlink(tmpfilename);
      sem_destroy(&(logptr->logsem));
      free(logptr);
    }
   }

 };

TEST_F(write_logTest, LogWriteOK) {
  int ret;
  FILE *fptr;
  char tmpstr[100], tmpstr1[100], tmpstr2[100];

  ret = open_log(tmpfilename);
  ASSERT_EQ(0, ret);
  LOG_LEVEL = 10;
  write_log(10, "Thisisatest");

  close_log();

  fptr = fopen(tmpfilename, "r");
  ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
  fclose(fptr);
  unlink(tmpfilename);
  ASSERT_EQ(3, ret);
  EXPECT_STREQ("Thisisatest", tmpstr2);
 }
TEST_F(write_logTest, NoLog) {
  int ret;
  FILE *fptr;
  char tmpstr[100], tmpstr1[100], tmpstr2[100];

  ret = open_log(tmpfilename);
  ASSERT_EQ(0, ret);
  LOG_LEVEL = 0;
  write_log(10, "Thisisatest");

  close_log();

  fptr = fopen(tmpfilename, "r");
  ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
  fclose(fptr);
  unlink(tmpfilename);
  EXPECT_EQ(EOF, ret);
 }
TEST_F(write_logTest, NoLogWriteOK) {
  int ret;
  FILE *fptr;
  char tmpstr[100], tmpstr1[100], tmpstr2[100];

  fptr = fopen(tmpfilename, "w");

  dup2(fileno(fptr), fileno(stdout));
  LOG_LEVEL = 10;
  write_log(10, "Thisisatest");
  fclose(fptr);

  fptr = fopen(tmpfilename, "r");
  ret = fscanf(fptr, "%s %s\t%s\n", tmpstr, tmpstr1, tmpstr2);
  fclose(fptr);
  unlink(tmpfilename);
  ASSERT_EQ(3, ret);
  EXPECT_STREQ("Thisisatest", tmpstr2);
 }

/* End of the test case for the function write_log */

