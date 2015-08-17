#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

void main(int argc, char **argv)
 {
  int fd,fd1,size_msg, status;
  struct sockaddr_un addr;
  char buf[4096];

  addr.sun_family = AF_UNIX;
  strcpy(addr.sun_path, "/dev/shm/hcfs_reporter");
  fd=socket(AF_UNIX, SOCK_STREAM,0);
  status = connect(fd,&addr,sizeof(addr));
  printf("status is %d, err %s\n", status,strerror(errno));
  printf("%s\n",argv[1]);
  size_msg=send(fd,argv[1],strlen(argv[1])+1,0);
  printf("sent %d bytes\n",size_msg);
  if (strcmp(argv[1],"stat")==0)
   {
    recv(fd,buf,4096,0);
   }
  printf("%s\n",buf);
  close(fd);
  return;
 }
