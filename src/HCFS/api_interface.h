/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: api_interface.h
* Abstract: The header file for Defining API for controlling / monitoring
*
* Revision History
* 2015/6/10 Jiahong created this file.
*
**************************************************************************/

#ifndef GW20_API_INTERFACE_H_
#define GW20_API_INTERFACE_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>

/* List of API codes */
#define TERMINATE 0
#define VOLSTAT 1

/* Message format for an API request:
	(From the first byte)
	API code, size (unsigned int)
	Total length of arguments, size (unsigned int)
	Arguments (To be stored in a char array and pass to the handler
		for each API)

   Message format for an API response:
	(From the first byte)
	Total length of response, size (unsigned int)
	Response (as a char string)
*/

typedef struct {
	struct sockaddr_un addr;
	int fd;
} SOCKET;

SOCKET api_sock;

pthread_t api_local_thread;  /* API thread (using local socket) */

int init_api_interface(void);
int destroy_api_interface(void);
void api_module(void);

#endif  /* GW20_API_INTERFACE_H_ */
