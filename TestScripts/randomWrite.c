#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define blocksize 65536

void main(int argc, void** argv){

	FILE * fd;

	int fileSize = 2048000; //in kB
	int iteration = 10240; 
	int randomSeek;
	char block[blocksize];
	int start, end;
	int rate;

	srand(time(NULL));

	fd =fopen("./test.img", "w+");
	fileSize = fileSize/(blocksize/1024); 


	start =time(NULL);	
	for(int i=0; i<	iteration; i++){

		randomSeek = (rand()%fileSize)*blocksize;
		fseek(fd, randomSeek, SEEK_SET);
		fwrite(block, blocksize, 1, fd);

	}
	fclose(fd);
	end =time(NULL);
	rate = blocksize*iteration/(end-start);
	
	printf("Transfer rate = %d KB/sec \n", rate/1024);
}
	
