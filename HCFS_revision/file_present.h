

int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat);
//int inode_stat_from_meta(ino_t this_inode, struct stat *inode_stat);

int mknod_update_meta(ino_t self_inode, ino_t parent_inode, char *selfname, struct stat *this_stat);
int mkdir_update_meta(ino_t self_inode, ino_t parent_inode, char *selfname, struct stat *this_stat);
int unlink_update_meta(ino_t parent_inode, ino_t this_inode,char *selfname);

int meta_forget_inode(ino_t self_inode);

