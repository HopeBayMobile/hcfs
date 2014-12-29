/*BEGIN string utility definition*/
int fetch_meta_path(char *pathname, ino_t this_inode);   /*Will copy the filename of the meta file to pathname*/
int fetch_block_path(char *pathname, ino_t this_inode, long long block_num);   /*Will copy the filename of the block file to pathname*/
int parse_parent_self(const char *pathname, char *parentname, char *selfname);
int fetch_todelete_path(char *pathname, ino_t this_inode);   /*Will copy the filename of the meta file in todelete folder to pathname*/

/*END string utility definition*/


int read_system_config(char *config_path);
int validate_system_config();

off_t check_file_size(const char *path);
