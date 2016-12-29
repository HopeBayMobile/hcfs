/*************************************************************************
*
* Copyright © 2016-2017 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: hcfscurl.h
* Abstract: The c header file for CURL operations.
*
* Revision History
* 2015/2/17 Jiahong added header for this file, and revising coding style.
*
**************************************************************************/

#ifndef GW20_HCFS_GOOGLEDRIVE_CURL_H_
#define GW20_HCFS_GOOGLEDRIVE_CURL_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <semaphore.h>
#include <pthread.h>
#include "objmeta.h"
#include "params.h"
#include "hcfscurl.h"

BACKEND_TOKEN_CONTROL *googledrive_token_control;
char googledrive_token[1024];

#endif
