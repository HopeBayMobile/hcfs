#include "mycurl.h"

int main(int argc, char **argv)
 {
  FILE *fptr;

  if (argc != 2)
   {
    printf("usage incorrect\n");
    exit(-1);
   }

  if (init_swift_backend()==0)
   {
    printf("\n\n\n test list begins\n\n\n");
//    swift_list_container();
    printf("uploading a file %s\n\n\n", argv[1]);
    fptr=fopen(argv[1],"r");
    swift_put_object(fptr, argv[1]);
    fclose(fptr);
    printf("end uploading \n\n\n");
//    swift_list_container();
    printf("test test\n\n");
//    swift_list_container();
    destroy_swift_backend();
   }
  return 0;
 }
