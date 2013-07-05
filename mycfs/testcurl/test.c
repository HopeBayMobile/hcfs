#include "mycurl.h"

int main(void)
 {
  FILE *fptr;
  if (init_swift_backend()==0)
   {
    printf("\n\n\n test list begins\n\n\n");
    swift_list_container();
    printf("uploading a file\n\n\n");
    fptr=fopen("test_put_file","r");
    swift_put_object(fptr, "test_put_file");
    fclose(fptr);
    printf("end uploading \n\n\n");
    swift_list_container();
    printf("test test\n\n");
    swift_list_container();
    destroy_swift_backend();
   }
  return 0;
 }
