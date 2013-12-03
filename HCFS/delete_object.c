#include "myfuse.h"
#include "mycurl.h"


/*TODO: Flow for delete loop will be similar to that of upload loop. Will first scan the to_be_deleted linked list in super inode and then check if the meta is in the to_delete temp dir. Open it and and start to delete the blocks first, then delete the meta last.*/

/*TODO: before actually moving the inode from to_be_deleted to deleted, must first check the upload threads and sync threads to find out if there are any pending uploads. It must wait until those are cleared. It must then wait for any additional pending meta or block deletion for this inode to finish.*/


void object_deletion_loop()
 {
  /*TODO: Will need multiple curl handles here also*/
  /*TODO: This will run in a separate process or thread?*/
  /* TODO: Perhaps this belongs in datasync.c, and use the curl handles there?*/
 }
