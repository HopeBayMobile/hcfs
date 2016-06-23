/*************************************************************************
*
* Copyright © 2016 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: common_stat.c
* Abstract: The source code provide an extra data layer for sys/stat.h .
*           All functions access struct hcfs_stat now should access through
*           functions provided here. Through this layer, common stat data
*           are stored at remote server. Eveantually file meta can be
*           sync accross platforms without handling different stat
*           structure everywhere.
*
* Revision History
* 2016/3/22 Jethro add common_stat.c and common_stat.h
*
**************************************************************************/

#include "hcfs_stat.h"
#include <stdio.h>

