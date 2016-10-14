#include <stdio.h>
#include <sys/types.h>

#define NO_PARENT_INO 5
#define ONE_PARENT_INO 6
#define FAKE_EXIST_PARENT 4
#define FAKE_ROOT 2
#define FAKE_GRAND_PARENT 3
#define MOCK_META_PATH "/tmp/this_meta"
#define MOCK_DIRSTAT_PATH "/tmp/dirstat"
#define MOCK_PATHLOOKUP_PATH "/tmp/pathlookup"

int32_t fake_num_parents;

