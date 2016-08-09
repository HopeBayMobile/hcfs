#include <vector>
#include <ftw.h>
#include "mock_params.h"
#include "gtest/gtest.h"
extern "C" {
#include "meta_mem_cache.h"
#include "filetables.h"
#include "global.h"
#include <errno.h>
}

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
	Unittest for init_system_fh_table()
 */
TEST(init_system_fh_tableTest, InitSuccess)
{
	/* Generate answer */
	int32_t val;
	char *flags_ans = (char *)malloc(sizeof(char) * MAX_OPEN_FILE_ENTRIES);
	FH_ENTRY *entry_table_ans =
		 (FH_ENTRY *)malloc(sizeof(FH_ENTRY) * MAX_OPEN_FILE_ENTRIES);
	memset(flags_ans, 0, sizeof(char) * MAX_OPEN_FILE_ENTRIES);
	memset(entry_table_ans, 0, sizeof(FH_ENTRY) * MAX_OPEN_FILE_ENTRIES);
	/* Run function */
	ASSERT_EQ(0, init_system_fh_table()) << "Testing init_system_fh_table() should success";
	/* Check answer*/
	EXPECT_EQ(0, memcmp(system_fh_table.entry_table_flags, flags_ans, 
		sizeof(char) * MAX_OPEN_FILE_ENTRIES));
	EXPECT_EQ(0, memcmp(system_fh_table.entry_table, entry_table_ans, 
		sizeof(FH_ENTRY) * MAX_OPEN_FILE_ENTRIES));
	EXPECT_EQ(0, system_fh_table.num_opened_files);
	EXPECT_EQ(0, system_fh_table.last_available_index);
	sem_getvalue(&(system_fh_table.fh_table_sem), &val);
	EXPECT_EQ(1, val);
}
/*
	End of unittest for init_system_fh_table()
 */

/*
	Unittest for open_fh()
 */
class open_fhTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			init_system_fh_table();
		}

};

TEST_F(open_fhTest, num_opened_files_LimitExceeded)
{
	/* Mock data */
	system_fh_table.num_opened_files = MAX_OPEN_FILE_ENTRIES + 1;
	/* Run function & Test */
	ASSERT_EQ(-EMFILE, open_fh(1, 0, FALSE)) << "Testing open_fh() should fail";
	/* Recover */
	system_fh_table.num_opened_files = 0;
}

TEST_F(open_fhTest, OpenfhSuccessFile)
{
	ino_t inode;
	int32_t index;
	for (int32_t times = 0 ; times < 500 ; times++) {
		/* Mock inode number */
		srand(time(NULL));
		inode = times;
		/* Run function */
		index = open_fh(inode, 0, FALSE) ;
		/* Check answer */
		ASSERT_GE(index, 0) << "Fail with inode = " << inode;
		ASSERT_EQ(IS_FH, system_fh_table.entry_table_flags[index]);
		ASSERT_EQ(inode, system_fh_table.entry_table[index].thisinode);
		ASSERT_EQ(times + 1, system_fh_table.num_opened_files);
	}
}
TEST_F(open_fhTest, OpenfhSuccessDir)
{
	ino_t inode;
	int index;
	for (int times = 0 ; times < 500 ; times++) {
		/* Mock inode number */
		srand(time(NULL));
		inode = times;
		/* Run function */
		index = open_fh(inode, 0, TRUE) ;
		/* Check answer */
		ASSERT_GE(index, 0) << "Fail with inode = " << inode;
		ASSERT_EQ(IS_DIRH, system_fh_table.entry_table_flags[index]);
		ASSERT_EQ(inode, system_fh_table.direntry_table[index].thisinode);
		ASSERT_EQ(times + 1, system_fh_table.num_opened_files);
	}
}

/*
	End of unittest for open_fh()
 */

/*
	Unittest for close_fh()
 */
class close_fhTest : public ::testing::Test {
	protected:
		virtual void SetUp()
		{
			init_system_fh_table();
		}
};

TEST_F(close_fhTest, CloseEmptyEntry) 
{
	ASSERT_EQ(-1, close_fh(0));
}

TEST_F(close_fhTest, meta_cache_lock_entry_ReturnNull)
{
	int32_t index;
	/* Mock data */
	index = open_fh(INO__META_CACHE_LOCK_ENTRY_FAIL, 0, FALSE);
	ASSERT_NE(-1, index);
	/* Test */
	ASSERT_EQ(0, close_fh(index));
}

TEST_F(close_fhTest, CloseSuccess)
{
	std::vector<int32_t> index_list;
	int32_t ans_num_opened_files = 0;
	uint32_t i;
	int32_t index;

	init_system_fh_table();
	/* Mock data */
	for (int32_t num_inode = 0; num_inode < 500 ; num_inode++) {
		int32_t index;
		int32_t inode = num_inode * 27;
		index = open_fh(inode, 0, FALSE);
		ASSERT_NE(-1, index) << "Fail to open fh with inode " << inode;
		index_list.push_back(index);
		ans_num_opened_files++;
	}
	/* Test */
	for (i = 0 ; i < index_list.size() ; i++) {
		index = index_list[i];
		ASSERT_EQ(0, close_fh(index)) << "Fail to open fh with inode " << index;
		ASSERT_EQ(FALSE, system_fh_table.entry_table_flags[index]);
		ASSERT_EQ(--ans_num_opened_files, system_fh_table.num_opened_files);
	}
}
/*
	End of unittest for close_fh()
 */

/* Begin of the test case for the function handle_dirmeta_snapshot */
class handle_dirmeta_snapshotTest : public ::testing::Test {
  protected:
    FILE *tmpfptr;
    char *tmpmeta;
    int errcode, ret;
    virtual void SetUp() {
      system_config = (SYSTEM_CONF_STRUCT *) malloc(sizeof(SYSTEM_CONF_STRUCT));
      memset(system_config, 0, sizeof(SYSTEM_CONF_STRUCT));
      METAPATH = (char *) malloc(100);
      tmpmeta = (char *) malloc(100);
      snprintf(METAPATH, 100, "/tmp/test_snapshot");
      snprintf(tmpmeta, 100, "%s/tmpmeta", METAPATH);
      ret = mkdir(METAPATH, 0700);
      if (ret != 0) {
        errcode = errno;
        printf("%d\n", errcode);
      }
      init_system_fh_table();
      system_fh_table.have_nonsnap_dir = FALSE;
      ret = mknod(tmpmeta, 0700 | S_IFREG, 0);
      if (ret != 0) {
        errcode = errno;
        printf("%d\n", errcode);
      }
      tmpfptr = fopen(tmpmeta, "r+");
    }
    virtual void TearDown() {
      if (tmpfptr != NULL)
        fclose(tmpfptr);
      unlink(tmpmeta);
      nftw(METAPATH, do_delete, 20, FTW_DEPTH);
      free(METAPATH);
      free(system_config);
    }

};

TEST_F(handle_dirmeta_snapshotTest, NoOpenedMetaFile) {

  system_fh_table.have_nonsnap_dir = FALSE;
  ASSERT_EQ(-EIO, handle_dirmeta_snapshot(0, NULL));
}

TEST_F(handle_dirmeta_snapshotTest, AllSnapshotCreated) {

  system_fh_table.have_nonsnap_dir = FALSE;
  ASSERT_EQ(0, handle_dirmeta_snapshot(0, tmpfptr));
}

TEST_F(handle_dirmeta_snapshotTest, NoOpenedFilesOrDir) {
  int count;

  system_fh_table.have_nonsnap_dir = TRUE;
  for (count = 0; count < MAX_OPEN_FILE_ENTRIES; count++)
    system_fh_table.entry_table_flags[count] = NO_FH;
  ASSERT_EQ(0, handle_dirmeta_snapshot(0, tmpfptr));
  ASSERT_EQ(FALSE, system_fh_table.have_nonsnap_dir);
}

TEST_F(handle_dirmeta_snapshotTest, OnlyOpenedFiles) {
  int count;

  system_fh_table.have_nonsnap_dir = TRUE;
  for (count = 0; count < MAX_OPEN_FILE_ENTRIES; count++)
    system_fh_table.entry_table_flags[count] = IS_FH;
  ASSERT_EQ(0, handle_dirmeta_snapshot(0, tmpfptr));
  ASSERT_EQ(FALSE, system_fh_table.have_nonsnap_dir);
}

TEST_F(handle_dirmeta_snapshotTest, NoMatchedOpenedDir) {
  int count;

  system_fh_table.have_nonsnap_dir = TRUE;
  for (count = 0; count < MAX_OPEN_FILE_ENTRIES; count++) {
    system_fh_table.entry_table_flags[count] = IS_DIRH;
    system_fh_table.direntry_table[count].thisinode = 10;
    system_fh_table.direntry_table[count].snapshot_ptr = NULL;
  }
  ASSERT_EQ(0, handle_dirmeta_snapshot(9, tmpfptr));
  ASSERT_EQ(TRUE, system_fh_table.have_nonsnap_dir);
}

TEST_F(handle_dirmeta_snapshotTest, OneMatchedOpenedDir) {
  int count;
  FILE *fptr1;
  char tmpstr[100];

  system_fh_table.have_nonsnap_dir = TRUE;
  for (count = 0; count < MAX_OPEN_FILE_ENTRIES; count++) {
    system_fh_table.entry_table_flags[count] = IS_DIRH;
    system_fh_table.direntry_table[count].thisinode = 10;
    system_fh_table.direntry_table[count].snapshot_ptr = NULL;
  }
  system_fh_table.direntry_table[0].thisinode = 9;
  fprintf(tmpfptr, "testmetacontent\n");
  rewind(tmpfptr);
  EXPECT_EQ(0, handle_dirmeta_snapshot(9, tmpfptr));
  EXPECT_EQ(TRUE, system_fh_table.have_nonsnap_dir);
  EXPECT_NE(0, (system_fh_table.direntry_table[0].snapshot_ptr != NULL));
  fptr1 = system_fh_table.direntry_table[0].snapshot_ptr;
  tmpstr[0] = 0;
  fscanf(fptr1, "%s\n", tmpstr);
  fclose(fptr1);
  ASSERT_EQ(0, strcmp(tmpstr, "testmetacontent"));
}

TEST_F(handle_dirmeta_snapshotTest, TwoMatchedOpenedDir) {
  int count, index;
  FILE *fptr1;
  char tmpstr[100];

  system_fh_table.have_nonsnap_dir = TRUE;
  for (count = 0; count < MAX_OPEN_FILE_ENTRIES; count++) {
    system_fh_table.entry_table_flags[count] = IS_DIRH;
    system_fh_table.direntry_table[count].thisinode = 10;
    system_fh_table.direntry_table[count].snapshot_ptr = NULL;
  }
  index = MAX_OPEN_FILE_ENTRIES - 1;
  system_fh_table.direntry_table[0].thisinode = 9;
  system_fh_table.direntry_table[index].thisinode = 9;
  fprintf(tmpfptr, "testmetacontent\n");
  rewind(tmpfptr);
  EXPECT_EQ(0, handle_dirmeta_snapshot(9, tmpfptr));
  EXPECT_EQ(TRUE, system_fh_table.have_nonsnap_dir);
  EXPECT_NE(0, (system_fh_table.direntry_table[0].snapshot_ptr != NULL));
  fptr1 = system_fh_table.direntry_table[0].snapshot_ptr;
  tmpstr[0] = 0;
  fscanf(fptr1, "%s\n", tmpstr);
  fclose(fptr1);
  EXPECT_EQ(0, strcmp(tmpstr, "testmetacontent"));
  EXPECT_NE(0, (system_fh_table.direntry_table[index].snapshot_ptr != NULL));
  fptr1 = system_fh_table.direntry_table[index].snapshot_ptr;
  tmpstr[0] = 0;
  fscanf(fptr1, "%s\n", tmpstr);
  fclose(fptr1);
  ASSERT_EQ(0, strcmp(tmpstr, "testmetacontent"));
}

TEST_F(handle_dirmeta_snapshotTest, ManyMatchedOpenedDir) {
  int count, index;
  FILE *fptr1;
  char tmpstr[100];

  system_fh_table.have_nonsnap_dir = TRUE;
  for (count = 0; count < MAX_OPEN_FILE_ENTRIES; count++)
    system_fh_table.entry_table_flags[count] = IS_FH;
  for (count = 0; count < 100; count++) {
    system_fh_table.entry_table_flags[count] = IS_DIRH;
    system_fh_table.direntry_table[count].thisinode = 9;
    system_fh_table.direntry_table[count].snapshot_ptr = NULL;
  }
  fprintf(tmpfptr, "testmetacontent\n");
  rewind(tmpfptr);
  EXPECT_EQ(0, handle_dirmeta_snapshot(9, tmpfptr));
  EXPECT_EQ(FALSE, system_fh_table.have_nonsnap_dir);
  for (index = 0; index < 100; index++) {
    EXPECT_NE(0, (system_fh_table.direntry_table[index].snapshot_ptr != NULL));
    fptr1 = system_fh_table.direntry_table[index].snapshot_ptr;
    tmpstr[0] = 0;
    fscanf(fptr1, "%s\n", tmpstr);
    fclose(fptr1);
    EXPECT_EQ(0, strcmp(tmpstr, "testmetacontent"));
  }
}

/* End of the test case for the function handle_dirmeta_snapshot */

