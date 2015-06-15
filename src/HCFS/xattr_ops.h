
#ifndef GW20_XATTR_OPS_H_
#define GW20_XATTR_OPS_H_

#define MAX_KEY_SIZE 256 /* Max key length */
#define MAX_VALUE_BLOCK_SIZE 4096 /* Max value size per block */
#define MAX_KEY_ENTRY_PER_LIST 55 /* Max key entry of the sorted array */
#define MAX_KEY_HASH_ENTRY 64 /* Max hash table entries */

/* Define namespace of xattr */
#define USER 0
#define SYSTEM 1
#define SECURITY 2
#define TRUSTED 3

/* Struct of VALUE_BLOCK. Value of an extened attr is stored using linked 
   VALUE_BLOCK, and it will be reclaimed if xattr is removed. */
typedef struct {
	char content[MAX_VALUE_BLOCK_SIZE];
	long long next_block_pos;
} VALUE_BLOCK; 

/* A key entry includes key size, value size, the key string, and a file 
   offset pointing to first value block. */
typedef struct {
	unsigned key_size;
	unsigned value_size; 
	char key[MAX_KEY_SIZE];
	long long first_value_block_pos;
} KEY_ENTRY;

/* KEY_LIST includes an array sorted by key, and number of xattr.
   If the KEY_LIST is the first one, prev_list_pos is set to 0. If it is the 
   last one, then next_list_pos is set to 0. */
typedef struct {
	unsigned num_xattr;
	KEY_ENTRY key_list[MAX_KEY_ENTRY_PER_LIST];
	long long next_list_pos;
	long long prev_list_pos;
} KEY_LIST_PAGE;

/* NAMESPACE_PAGE includes a hash table which is used to hash the input key.
   Each hash entry points to a KEY_LIST. */
typedef struct {
	unsigned num_xattr;
	long long key_hash_table[MAX_KEY_HASH_ENTRY];
} NAMESPACE_PAGE;

/* XATTR_PAGE is pointed by next_xattr_page in meta file. Namespace is one of 
   user, system, security, and trusted. */
typedef struct {
	long long reclaimed_key_list;
	long long reclaimed_value_block;
	NAMESPACE_PAGE namespace_page[4]; 
} XATTR_PAGE;

int parse_xattr_namespace(const char *name, char *name_space, char *key);

#endif
