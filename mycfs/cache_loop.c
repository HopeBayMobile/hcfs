#include "myfuse.h"
#include "mycurl.h"


/*TODO: How to pick the victim to replace?*/
/*Only kick the blocks that's stored on cloud, i.e., stored_where ==3*/
/*TODO: How to identify?*/
void run_cache_loop()
 {

  while(1==1)
   {
    /*Sleep for 10 seconds if not triggered*/
    if (mysystem_meta.cache_size < CACHE_SOFT_LIMIT)
     {
      sleep(10);
      continue;
     }

    
   }
 }
