#include "fuseop.h"
#include "params.h"
#include "global.h"

int init_system_fh_table()
 {
  long long count;

  memset(&system_fh_table,0,sizeof(FH_TABLE_TYPE));

  system_fh_table.entry_table_flags = malloc(sizeof(char) * MAX_OPEN_FILE_ENTRIES);
  if (system_fh_table.entry_table_flags == NULL)
   return -1;
  memset(system_fh_table.entry_table_flags,0,sizeof(char) * MAX_OPEN_FILE_ENTRIES);

  system_fh_table.entry_table = malloc(sizeof(FH_ENTRY) * MAX_OPEN_FILE_ENTRIES);
  if (system_fh_table.entry_table == NULL)
   return -1;

  memset(system_fh_table.entry_table,0,sizeof(FH_ENTRY) * MAX_OPEN_FILE_ENTRIES);

  sem_init(&(system_fh_table.fh_table_sem),0,1);
  return 0;
 }

long long open_fh(ino_t thisinode)
 {
  long long index;
  char thismetapath[METAPATHLEN];

  sem_wait(&(system_fh_table.fh_table_sem));

  if (system_fh_table.num_opened_files >= MAX_OPEN_FILE_ENTRIES)
   {
    sem_post(&(system_fh_table.fh_table_sem));
    return -1;   /*Not able to allocate any more fh entry as table is full.*/
   }

  index = system_fh_table.last_available_index;
  while(system_fh_table.entry_table_flags[index]==TRUE)
   {
    index++;
    index = index % MAX_OPEN_FILE_ENTRIES;
   }

  fetch_meta_path(thismetapath,thisinode);

  system_fh_table.entry_table_flags[index]=TRUE;
  system_fh_table.entry_table[index].thisinode = thisinode;
  system_fh_table.entry_table[index].metafptr = NULL;
  system_fh_table.entry_table[index].metafptr = fopen(thismetapath, "r+");

  setbuf(system_fh_table.entry_table[index].metafptr,NULL);
  fread(&(system_fh_table.entry_table[index].cached_meta),sizeof(FILE_META_TYPE),1,system_fh_table.entry_table[index].metafptr);
  fseek(system_fh_table.entry_table[index].metafptr,0,SEEK_SET);

  system_fh_table.entry_table[index].blockfptr = NULL;
  system_fh_table.entry_table[index].opened_block = -1;
  system_fh_table.entry_table[index].cached_page_index = -1;
  system_fh_table.entry_table[index].cached_page_start_fpos = 0;
  sem_init(&(system_fh_table.entry_table[index].block_sem),0,1);

  sem_post(&(system_fh_table.fh_table_sem));
  return index;
 }

int close_fh(long long index)
 {
  /*TODO: should not have dirty page and meta in this fh entry, but could add routine to check*/
  sem_wait(&(system_fh_table.fh_table_sem));

  if (system_fh_table.entry_table_flags[index]==TRUE)
   {
    system_fh_table.entry_table_flags[index]=FALSE;
    system_fh_table.entry_table[index].thisinode = 0;
    if (system_fh_table.entry_table[index].metafptr!=NULL)
     fclose(system_fh_table.entry_table[index].metafptr);
    if ((system_fh_table.entry_table[index].blockfptr!=NULL) && (system_fh_table.entry_table[index].opened_block>=0))
     fclose(system_fh_table.entry_table[index].blockfptr);

    system_fh_table.entry_table[index].metafptr = NULL;
    system_fh_table.entry_table[index].blockfptr = NULL;
    system_fh_table.entry_table[index].opened_block = -1;
    sem_destroy(&(system_fh_table.entry_table[index].block_sem));
   }

  sem_post(&(system_fh_table.fh_table_sem));
  return 0;
 }

int seek_page(FILE *fptr, FH_ENTRY *fh_ptr,long long target_page)
 {
  long long current_page;
  off_t nextfilepos, prevfilepos, currentfilepos;
  BLOCK_ENTRY_PAGE temppage;


  nextfilepos=fh_ptr->cached_meta.next_block_page;
  current_page = 0;
  prevfilepos = 0;

  /*TODO: put error handling for the read/write ops here*/
  while(current_page <= target_page)
   {
    if (nextfilepos == 0) /*Need to append a new block entry page */
     {
      if (prevfilepos == 0) /* If not even the first page is generated */
       {
        fseek(fptr, 0, SEEK_END);
        prevfilepos = ftell(fptr);
        fh_ptr->cached_meta.next_block_page = prevfilepos;
        memset(&temppage,0,sizeof(BLOCK_ENTRY_PAGE));
        fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
        fseek(fptr,0,SEEK_SET);
        fwrite(&(fh_ptr->cached_meta), sizeof(FILE_META_TYPE),1,fptr);
       }
      else
       {
        fseek(fptr, 0, SEEK_END);
        currentfilepos = ftell(fptr);
        fseek(fptr, prevfilepos, SEEK_SET);
        fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
        temppage.next_page = currentfilepos;
        fseek(fptr, prevfilepos, SEEK_SET);
        fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);

        fseek(fptr, currentfilepos, SEEK_SET);
        memset(&temppage,0,sizeof(BLOCK_ENTRY_PAGE));
        fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
        prevfilepos = currentfilepos;
       }
     }
    else
     {
      fseek(fptr, nextfilepos, SEEK_SET);
      prevfilepos = nextfilepos;
      fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
      nextfilepos = temppage.next_page;
     }
    if (current_page == target_page)
     break;
    else
     current_page++;
   }
  fh_ptr->cached_page_index = target_page;
  fh_ptr->cached_page_start_fpos = prevfilepos;
  return 0;
 }

long long advance_block(FILE *fptr, off_t thisfilepos,long long *entry_index)
 {
  long long temp_index;
  off_t nextfilepos;
  BLOCK_ENTRY_PAGE temppage;
  /*First handle the case that nothing needs to be changed, just add entry_index*/

  temp_index = *entry_index;
  if ((temp_index+1) < MAX_BLOCK_ENTRIES_PER_PAGE)
   {
    temp_index++;
    *entry_index = temp_index;
    return thisfilepos;
   }

  /*We need to change to another page*/

  fseek(fptr,thisfilepos,SEEK_SET);
  fread(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
  nextfilepos = temppage.next_page;

  if (nextfilepos == 0)   /*Need to allocate a new page*/
   {
    fseek(fptr,0,SEEK_END);
    nextfilepos = ftell(fptr);
    temppage.next_page = nextfilepos;
    fseek(fptr, thisfilepos,SEEK_SET);
    fwrite(&(temppage),sizeof(BLOCK_ENTRY_PAGE),1,fptr);
    fseek(fptr,nextfilepos,SEEK_SET);
    memset(&temppage,0,sizeof(BLOCK_ENTRY_PAGE));
    fwrite(&temppage,sizeof(BLOCK_ENTRY_PAGE),1,fptr);
   }

  *entry_index = 0;
  return nextfilepos;
 }
