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
#ifndef __STATVFS_H
#define __STATVFS_H

struct statvfs {
	unsigned long  f_bsize;    /* file system block size */
	unsigned long  f_frsize;   /* fragment size */
	fsblkcnt_t     f_blocks;   /* size of fs in f_frsize units */
	fsblkcnt_t     f_bfree;    /* # free blocks */
	fsblkcnt_t     f_bavail;   /* # free blocks for non-root */
	fsfilcnt_t     f_files;    /* # inodes */
	fsfilcnt_t     f_ffree;    /* # free inodes */
	fsfilcnt_t     f_favail;   /* # free inodes for non-root */
	unsigned long  f_fsid;     /* file system ID */
	unsigned long  f_flag;     /* mount flags */
	unsigned long  f_namemax;  /* maximum filename length */
};

#endif
