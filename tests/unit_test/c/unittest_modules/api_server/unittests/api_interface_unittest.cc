#include <stdio.h>
#include <semaphore.h>
#include <stdlib.h>
#include <attr/xattr.h>

#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
extern "C" {
#include "api_interface.h"
#include "global.h"
#include "hfuse_system.h"
#include "params.h"
}
#include "gtest/gtest.h"

