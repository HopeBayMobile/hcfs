#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "global.h"
#include "file_present.h"
#include "super_inode.h"



int fetch_inode_stat(ino_t this_inode, struct stat *inode_stat)
 {
  SUPER_INODE_ENTRY tempentry;
  int ret_code;

  /*TODO : inode stat caching? How? */
  if (this_inode > 0)
   {
    ret_code =super_inode_read(this_inode, &tempentry);    

    if (ret_code < 0)
     {
      /* TODO: For performance reason, may not want to try meta files. System could only test if some file really does not exist */
      /* TODO: This option should only be turned on if system is in some sort of recovery mode. In this case, should even try backend meta backups */

      //ret_code = inode_stat_from_meta(this_inode, inode_stat);
      /* TODO: Perhaps missing from super inode (inode caching?). Fill it in? */
     }
    else
     memcpy(inode_stat,&(tempentry.inode_stat),sizeof(struct stat));

    if (ret_code < 0)
     return -ENOENT;
   }
  else
   return -ENOENT;


  #if DEBUG >= 5
  printf("fetch_inode_stat %lld\n",inode_stat->st_ino);
  #endif  /* DEBUG */


  return 0;
 }

//int inode_stat_from_meta(ino_t this_inode, struct stat *inode_stat)

