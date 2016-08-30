#include <gtest/gtest.h>

extern "C" {
#include "fuse_notify.h"
#include "mount_manager.h"
#include <dlfcn.h>
#include <fuse/fuse_lowlevel.h>
#include <stdarg.h>
#include <stdarg.h>
#include <stdio.h>
}

/* Definition for fake functions */
#include "../../fff.h"
DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(int, sem_init, sem_t *, int, unsigned int);
typedef int (sem_init_f)(sem_t *, int, unsigned int);
sem_init_f *real_sem_init = (sem_init_f *)dlsym(RTLD_NEXT, "sem_init");

int32_t sem_init_call = 0;
int32_t sem_init_error_on = -1;
int32_t sem_init_cnt(sem_t *sem, int pshared, unsigned int value)
{
	ssize_t ret;

	sem_init_call += 1;
	printf("sem_init_call %d\n", sem_init_call);
	ret = real_sem_init(sem, pshared, value);
	return (sem_init_call == sem_init_error_on) ? -1 : ret;
}

void reset_fake_functions(void)
{
	FFF_RESET_HISTORY();

	RESET_FAKE(sem_init);
	sem_init_call = 0;
	sem_init_error_on = -1;
	sem_init_fake.custom_fake = real_sem_init;
}

/* gtests */
MOUNT_T_GLOBAL mount_global = {0};
extern FUSE_NOTIFY_CYCLE_BUF notify_cb;
extern fuse_notify_fn *notify_fn[];

int32_t write_log(int32_t level, char *format, ...)
{
	va_list alist;

	va_start(alist, format);
	vprintf(format, alist);
	va_end(alist);
	return 0;
}

class utEnvironment : public ::testing::Environment
{
	public:
	virtual void SetUp() {}
	virtual void TearDown() {}
};

::testing::Environment *const fuseop_env =
    ::testing::AddGlobalTestEnvironment(new utEnvironment);

class init_notify_cbTest : public ::testing::Test
{
	protected:
	virtual void SetUp()
	{
		reset_fake_functions();
	}

	virtual void TearDown() {}
};

TEST_F(init_notify_cbTest, Success)
{
	init_notify_cb();
	EXPECT_NE(notify_cb.elems, NULL);
	EXPECT_EQ(notify_cb.is_initialized, 1);
}
TEST_F(init_notify_cbTest, Fail)
{

	sem_init_fake.custom_fake = sem_init_cnt;
	sem_init_error_on = 2;
	init_notify_cb();
	EXPECT_EQ(notify_cb.elems, NULL);
	EXPECT_EQ(notify_cb.is_initialized, 0);
}
