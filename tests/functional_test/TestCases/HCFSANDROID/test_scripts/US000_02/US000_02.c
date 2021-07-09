/*
 * Copyright (c) 2021 HopeBayTech.
 *
 * This file is part of Tera.
 * See https://github.com/HopeBayMobile for further info.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

/*
 * THIS PART CAN BE RE-IMPLEEMNT INTO A HELPER FUNCTION THAT GRAB
 * RETURN VALUE FROM SYSTEM CALL.
 */
int sizeofDirty(void)
{
    FILE *fp;
    char ret[1024];
    char *token, *prevToken;

    fp = popen("/system/bin/HCFSvol dirtysize", "r");
    if (fp == NULL)
    {
        printf("Failed to run HCFSvol\n");
        exit(1);
    }
    fgets(ret, sizeof(ret)-1, fp);
    fgets(ret, sizeof(ret)-1, fp);

    token = strtok(ret, " \n");
    while(token != NULL){
        prevToken = token;
        token = strtok(NULL, " ");
   }

    int dirty_size = atoi((const char*)prevToken);

    pclose(fp);

    return dirty_size;
}


int backup_file(const char *path)
{
    FILE *fp;
    char ret[1024];
    char cmd_cp[1024] = "cp ";
    strcat(cmd_cp, path);
    strcat(cmd_cp, " ");
    strcat(cmd_cp, path);
    strcat(cmd_cp, ".backup");

    printf("> %s\n", cmd_cp);
    fp = popen(cmd_cp, "r");
    if (fp == NULL)
    {
        printf("Failed to backup file on %s\n", path);
        exit(1);
    }
    while (fgets(ret, sizeof(path)-1, fp) != NULL)
    {
        printf("%s", ret);
    }

    pclose(fp);

    return 0;
}


static void waitUntilSynced(void)
{
    // Wait until file is synced to background
    //  * how to check?
    //      <A> `HCFSvol dirtysize` return `0`

    int remainDirty;

    while ( (remainDirty = sizeofDirty()) != 0 )
    {
        printf("Current dirty size: %d\n", remainDirty);
        sleep(3);
    }
}



int main(void)
{
    int fd;
    int n;
    int count;
    int offset;
    char output_path[]="/sdcard/datablock";

    count = 3;
    offset = 0;
    /* First write attempt */
    if ((fd = creat(output_path, 0666)) < 0) {
        perror("open");
        return -1;
    }



    if ((n = pwrite(fd, "111", count, offset)) != count) {
        perror("pwrite");
        return -1;
    }

    printf("> Syncing write attempt 1 to backend...\n");
    waitUntilSynced();

    /* Backup file as <filename>.backup */
    backup_file(output_path);


    /* Second write attempt */
  /*  if ((fd=open(output_path, O_CREAT|O_RDWR, 0666)) < 0) {
        perror("open");
        return -1;
    }*/

    count = 3;
    offset = 0;
    if ((count = pwrite(fd, "222", count, offset)) != 3) {
        perror("pwrite");
        return -1;
    }

    printf("> Syncing write attempt 2 to backend...\n");
    waitUntilSynced();

    close(fd);
    return EXIT_SUCCESS;
}
