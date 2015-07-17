#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "mount_manager.h"
#include "fuseop.h"
#include "global.h"
#include "params.h"
#include "mount_manager_unittest.h"
}
#include "gtest/gtest.h"

SYSTEM_CONF_STRUCT system_config;

MOUNT_NODE_T* prepare_node(char *fname) {
  MOUNT_NODE_T *tmpptr;

  tmpptr = (MOUNT_NODE_T *) malloc(sizeof(MOUNT_NODE_T));
  memset(tmpptr, 0, sizeof(MOUNT_NODE_T));
  tmpptr->mt_entry = (MOUNT_T *) malloc(sizeof(MOUNT_T));
  memset(tmpptr->mt_entry, 0, sizeof(MOUNT_T));
  strcpy((tmpptr->mt_entry)->f_name, fname);
  return tmpptr;
 }

void free_node(MOUNT_NODE_T *innode) {
  free(innode->mt_entry);
  free(innode);
 }

void free_tree(MOUNT_NODE_T *node) {
  if (node->lchild != NULL)
    free_tree(node->lchild);

  if (node->rchild != NULL)
    free_tree(node->rchild);

  free_node(node);
 }

MOUNT_NODE_T* build_subtree(MOUNT_NODE_T *parent, int start1, int end1) {
  MOUNT_NODE_T *tmpnode;
  char fsname[10];
  int midval, start, end;

  midval = (start1 + end1) / 2;
  snprintf(fsname, 10, "%4d", midval);
  printf("building %s\n", fsname);

  tmpnode = prepare_node(fsname);
  tmpnode->parent = parent;

  start = start1;
  end = midval - 1;
  if (start <= end)
    tmpnode->lchild = build_subtree(tmpnode, start, end);
  else
    tmpnode->lchild = NULL;

  start = midval + 1;
  end = end1;
  if (start <= end)
    tmpnode->rchild = build_subtree(tmpnode, start, end);
  else
    tmpnode->rchild = NULL;

  return tmpnode;
 }

void build_tree(int num_nodes) {
  mount_mgr.root = build_subtree(NULL, 0, num_nodes - 1);
 }

/* Begin of the test case for the function search_mount */

class search_mountTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
   }
 };

TEST_F(search_mountTest, EmptyDatabase) {
  int ret;
  MOUNT_T *tmpinfo;

  mount_mgr.root = NULL;
  ret = search_mount("anyFS", &tmpinfo);
  EXPECT_EQ(-ENOENT, ret);

 }

TEST_F(search_mountTest, MultipleFSFound) {
  int ret, count;
  MOUNT_T *tmpinfo;
  char fsname[10];

  build_tree(100);

  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = search_mount(fsname, &tmpinfo);
    EXPECT_EQ(0, ret);
   }

 }
TEST_F(search_mountTest, MultipleFSNotFound) {
  int ret, count;
  MOUNT_T *tmpinfo;
  char fsname[10];

  build_tree(100);

  for (count = 100; count < 200; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = search_mount(fsname, &tmpinfo);
    EXPECT_EQ(-ENOENT, ret);
   }

 }

/* End of the test case for the function search_mount */

/* Begin of the test case for the function insert_mount */

class insert_mountTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
    mount_mgr.num_mt_FS = 0;
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
   }
 };

TEST_F(insert_mountTest, MultipleFS) {
  int ret, count;
  MOUNT_T *tmpinfo1, *tmpinfo2;
  MOUNT_T *tmpptr[100];
  char fsname[10];

  for (count = 0; count < 100; count++) {
    tmpinfo1 = (MOUNT_T *) malloc(sizeof(MOUNT_T));
    snprintf(fsname, 10, "%4d", count);
    strcpy(tmpinfo1->f_name, fsname);
    ret = insert_mount(fsname, tmpinfo1);
    if (ret != 0)
      printf("%s\n", strerror(-ret));
    ASSERT_EQ(0, ret);
    tmpptr[count] = tmpinfo1;
   }
  ASSERT_EQ(100, mount_mgr.num_mt_FS);
  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = search_mount(fsname, &tmpinfo2);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(tmpptr[count], tmpinfo2);
   }

 }
TEST_F(insert_mountTest, MultipleFSReverse) {
  int ret, count;
  MOUNT_T *tmpinfo1, *tmpinfo2;
  MOUNT_T *tmpptr[100];
  char fsname[10];

  for (count = 99; count >= 0; count--) {
    tmpinfo1 = (MOUNT_T *) malloc(sizeof(MOUNT_T));
    snprintf(fsname, 10, "%4d", count);
    strcpy(tmpinfo1->f_name, fsname);
    ret = insert_mount(fsname, tmpinfo1);
    if (ret != 0)
      printf("%s\n", strerror(-ret));
    ASSERT_EQ(0, ret);
    tmpptr[count] = tmpinfo1;
   }
  ASSERT_EQ(100, mount_mgr.num_mt_FS);
  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = search_mount(fsname, &tmpinfo2);
    ASSERT_EQ(0, ret);
    EXPECT_EQ(tmpptr[count], tmpinfo2);
   }

 }

/* End of the test case for the function insert_mount */

/* Begin of the test case for the function delete_mount */

class delete_mountTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    hcfs_system = (SYSTEM_DATA_HEAD *) malloc(sizeof(SYSTEM_DATA_HEAD));
    mount_mgr.root = NULL;
    mount_mgr.num_mt_FS = 0;
   }

  virtual void TearDown() {
    if (mount_mgr.root != NULL)
      free_tree(mount_mgr.root);
    free(hcfs_system);
   }
 };

TEST_F(delete_mountTest, MultipleFS) {
  int ret, count;
  MOUNT_T *tmpinfo1;
  MOUNT_NODE_T *tmpnode;
  char fsname[10];

  build_tree(100);
  mount_mgr.num_mt_FS = 100;

  for (count = 0; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    ret = delete_mount(fsname, &tmpnode);
    ASSERT_EQ(0, ret);
    EXPECT_STREQ(fsname, (tmpnode->mt_entry)->f_name);
    free(tmpnode->mt_entry);
    free(tmpnode);
    ret = search_mount(fsname, &tmpinfo1);
    EXPECT_EQ(-ENOENT, ret);
   }
  ASSERT_EQ(0, mount_mgr.num_mt_FS);

 }

TEST_F(delete_mountTest, MultipleFSReverse) {
  int ret, count;
  MOUNT_T *tmpinfo1;
  MOUNT_NODE_T *tmpnode;
  char fsname[10];

  build_tree(100);
  mount_mgr.num_mt_FS = 100;

  for (count = 99; count >= 0; count--) {
    snprintf(fsname, 10, "%4d", count);
    ret = delete_mount(fsname, &tmpnode);
    ASSERT_EQ(0, ret);
    EXPECT_STREQ(fsname, (tmpnode->mt_entry)->f_name);
    free(tmpnode->mt_entry);
    free(tmpnode);
    ret = search_mount(fsname, &tmpinfo1);
    EXPECT_EQ(-ENOENT, ret);
   }
  ASSERT_EQ(0, mount_mgr.num_mt_FS);

 }

TEST_F(delete_mountTest, MultipleFSReverse1) {
  int ret, count;
  MOUNT_T *tmpinfo1;
  MOUNT_NODE_T *tmpnode;
  char fsname[10];

  build_tree(100);
  mount_mgr.num_mt_FS = 100;

  for (count = 49; count >= 0; count--) {
    snprintf(fsname, 10, "%4d", count);
    printf("deleting %s\n", fsname);
    ret = delete_mount(fsname, &tmpnode);
    ASSERT_EQ(0, ret);
    EXPECT_STREQ(fsname, (tmpnode->mt_entry)->f_name);
    free(tmpnode->mt_entry);
    free(tmpnode);
    ret = search_mount(fsname, &tmpinfo1);
    EXPECT_EQ(-ENOENT, ret);
   }
  ASSERT_EQ(50, mount_mgr.num_mt_FS);

 }

TEST_F(delete_mountTest, MultipleFS1) {
  int ret, count;
  MOUNT_T *tmpinfo1;
  MOUNT_NODE_T *tmpnode;
  char fsname[10];

  build_tree(100);
  mount_mgr.num_mt_FS = 100;

  for (count = 49; count < 100; count++) {
    snprintf(fsname, 10, "%4d", count);
    printf("deleting %s\n", fsname);
    ret = delete_mount(fsname, &tmpnode);
    ASSERT_EQ(0, ret);
    EXPECT_STREQ(fsname, (tmpnode->mt_entry)->f_name);
    free(tmpnode->mt_entry);
    free(tmpnode);
    ret = search_mount(fsname, &tmpinfo1);
    EXPECT_EQ(-ENOENT, ret);
   }
  ASSERT_EQ(49, mount_mgr.num_mt_FS);

 }

/* End of the test case for the function delete_mount */

