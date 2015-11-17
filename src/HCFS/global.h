/*************************************************************************
*
* Copyright © 2014-2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: global.h
* Abstract: The header file for global settings for HCFS
*
* Revision History
* 2015/2/11 Jiahong added header for this file and revised coding style.
* 2015/7/21 Jiahong moving API codes to global.h
*
**************************************************************************/

#ifndef GW20_SRC_GLOBAL_H_
#define GW20_SRC_GLOBAL_H_

#define TRUE 1
#define FALSE 0

/* Defines the version of the current meta defs */
#define CURRENT_META_VER 3
#define BACKWARD_COMPATIBILITY 3
/* TODO: force backward compability check when reading meta file */

#define X64 1
#define ARM_32BIT 2
#define ANDROID_32BIT 3

#define ARCH_CODE ANDROID_32BIT

/* List of API codes */
#define TERMINATE 0
#define VOLSTAT 1
#define TESTAPI 2
#define ECHOTEST 3
#define CREATEFS 4
#define MOUNTFS 5
#define DELETEFS 6
#define CHECKFS 7
#define LISTFS 8
#define UNMOUNTFS 9
#define CHECKMOUNT 10
#define UNMOUNTALL 11
#define CLOUDSTAT 12

#define HTTP_200_OK                        200
#define HTTP_201_CREATED                   201
#define HTTP_202_ACCEPTED                  202
#define HTTP_204_NO_CONTENT                204
#define HTTP_206_PARTIAL_CONTENT           206

#define HTTP_301_MOVED_PERMANENTLY         301
#define HTTP_302_MOVED_TEMPORARILY         302
#define HTTP_302_REDIRECT                  302
#define HTTP_303_SEE_OTHER                 303
#define HTTP_304_NOT_MODIFIED              304
#define HTTP_307_TEMPORARY_REDIRECT        307

#define HTTP_400_BAD_REQUEST               400
#define HTTP_401_UNAUTHORIZED              401
#define HTTP_402_PAYMENT_REQUIRED          402
#define HTTP_403_FORBIDDEN                 403
#define HTTP_404_NOT_FOUND                 404
#define HTTP_405_NOT_ALLOWED               405
#define HTTP_406_NOT_ACCEPTABLE            406
#define HTTP_408_REQUEST_TIME_OUT          408
#define HTTP_409_CONFLICT                  409
#define HTTP_410_GONE                      410
#define HTTP_411_LENGTH_REQUIRED           411
#define HTTP_413_REQUEST_ENTITY_TOO_LARGE  413
#define HTTP_414_REQUEST_URI_TOO_LARGE     414
#define HTTP_415_UNSUPPORTED_MEDIA_TYPE    415
#define HTTP_416_RANGE_NOT_SATISFIABLE     416

#define HTTP_500_INTERNAL_SERVER_ERROR     500
#define HTTP_500_SERVER_ERROR              500
#define HTTP_501_NOT_IMPLEMENTED           501
#define HTTP_502_BAD_GATEWAY               502
#define HTTP_503_SERVICE_UNAVAILABLE       503
#define HTTP_504_GATEWAY_TIME_OUT          504
#define HTTP_507_INSUFFICIENT_STORAGE      507

#endif  /* GW20_SRC_GLOBAL_H_ */
