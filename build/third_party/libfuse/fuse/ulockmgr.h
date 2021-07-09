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

#include <stdint.h>
#include <fcntl.h>
#include <sys/types.h>

/**
 * Perform POSIX locking operation
 *
 * @param fd the file descriptor
 * @param cmd the locking command (F_GETFL, F_SETLK or F_SETLKW)
 * @param lock the lock parameters
 * @param owner the lock owner ID cookie
 * @param owner_len length of the lock owner ID cookie
 * @return 0 on success -errno on error
 */
int ulockmgr_op(int fd, int cmd, struct flock *lock, const void *owner,
		size_t owner_len);
