> void fetch_from_cloud(FILE *fptr, ino_t this_inode, long block_no)
>  {
>   char objname[1000];
>   int status;
>   int which_curl_handle;
> 
>   sprintf(objname,"data_%ld_%ld",this_inode,block_no);
>   while(1==1)
>    {
>     sem_wait(&download_curl_sem);
>     for(which_curl_handle=0;which_curl_handle<MAX_CURL_HANDLE;which_curl_handle++)
>      {
>       if (curl_handle_mask[which_curl_handle] == False)
>        {
>         curl_handle_mask[which_curl_handle] = True;
>         break;
>        }
>      }
>     printf("Debug datasync: downloading using curl handle %d\n",which_curl_handle);
>     status=swift_get_object(fptr,objname,download_curl_handles[which_curl_handle].curl);
>     curl_handle_mask[which_curl_handle] = False;
>     sem_post(&download_curl_sem);
>     if (status!=0)
>      {
>       if (swift_reauth()!=0)
>        {
>         printf("Debug datasync: error in connecting to swift\n");
>         exit(0);
>        }
>      }
>     else
>      break;
>    }
> 
>   fflush(fptr);
>   return;
>  }

