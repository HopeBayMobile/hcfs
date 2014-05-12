#include <stdio.h>
#include <stdlib.h>

void main()
 {
  FILE *fptr;
  char buf[20000];

  fptr = fopen("/storage/home/jiahongwu/HCFS/blockstorage/sub_3/block2_1","r");
  fread(buf,1,131072,fptr);
  fclose(fptr);
  return;
 }
