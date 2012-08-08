#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#define blocksize 65536
#define filesize 2048000

int blockNum = filesize/(blocksize/1024);  
int iteration = 512;

void *thread_function(void *arg);

int main(){


	int id[8]={0,1,2,3,4,5,6,7};
	pthread_t threads[8];
	void *thread_result;

	pthread_create(&threads[0], NULL, thread_function, &id[0]); 
	pthread_create(&threads[1], NULL, thread_function, &id[1]); 
	
	pthread_create(&threads[2], NULL, thread_function, &id[2]); 
	pthread_create(&threads[3], NULL, thread_function, &id[3]); 
	pthread_create(&threads[4], NULL, thread_function, &id[4]); 
	pthread_create(&threads[5], NULL, thread_function, &id[5]); 
	pthread_create(&threads[6], NULL, thread_function, &id[6]); 
	pthread_create(&threads[7], NULL, thread_function, &id[7]); 
	
	pthread_join(threads[0], &thread_result);
	pthread_join(threads[1], &thread_result);
	pthread_join(threads[2], &thread_result);
	pthread_join(threads[2], &thread_result);
	pthread_join(threads[3], &thread_result);
	pthread_join(threads[4], &thread_result);
	pthread_join(threads[5], &thread_result);
	pthread_join(threads[6], &thread_result);
	pthread_join(threads[7], &thread_result);

}

void* thread_function(void* arg){

	FILE * fd;

	int id = *((int*)arg);
	int randomSeek;
	char block[blocksize];
	int start, end;
	int rate;
	char filename[1024]; 
	
	sprintf(filename, "./test%d.img", id);
	printf("filename is %s\n", filename);

	srand(time(NULL));

	fd =fopen(filename, "w+");

	printf("mod is %d \n", blockNum);

	start =time(NULL);	
	for(int i=0; i<	iteration; i++){

		randomSeek = (rand()%blockNum)*blocksize;
		fseek(fd, randomSeek, SEEK_SET);
		fwrite(block, blocksize, 1, fd);

	}
	fclose(fd);
	end =time(NULL);
	rate = blocksize*iteration/(end-start);
	
	printf("Transfer rate %d = %d KB/sec \n", id, rate/1024);
	pthread_exit("OK");
}
	
