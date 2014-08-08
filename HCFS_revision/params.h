/* Finish the system config struct, then point the macros to the system config struct obj */
/* Will need a config parser function and a config validator function */

typedef struct {
  char *metapath;
  char *blockpath;
  char *superinode_name;
  char *unclaimed_name;
  char *hcfssystem_name;
  long long cache_soft_limit;
  long long cache_hard_limit;
  long long cache_update_delta;
  long long max_block_size;
 } SYSTEM_CONF_STRUCT;

SYSTEM_CONF_STRUCT system_config;

#define METAPATH system_config.metapath
#define BLOCKPATH system_config.blockpath
#define SUPERINODE system_config.superinode_name
#define UNCLAIMEDFILE system_config.unclaimed_name
#define HCFSSYSTEM system_config.hcfssystem_name

#define CACHE_SOFT_LIMIT system_config.cache_soft_limit
#define CACHE_HARD_LIMIT system_config.cache_hard_limit
#define CACHE_DELTA system_config.cache_update_delta

#define MAX_BLOCK_SIZE system_config.max_block_size

#define METAPATHLEN 400

#define NUMSUBDIR 1000

#define RECLAIM_TRIGGER 10000

#define NO_LL 0
#define IS_DIRTY 1
#define TO_BE_DELETED 2
#define TO_BE_RECLAIMED 3
#define RECLAIMED 4

#define DEFAULT_CONFIG_PATH "/etc/hcfs.conf"

int read_system_config(char *config_path);
int validate_system_config();

