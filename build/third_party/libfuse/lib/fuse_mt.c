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

#include "fuse_i.h"
#include "fuse_misc.h"
#include "fuse/fuse_lowlevel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

struct procdata {
	struct fuse *f;
	struct fuse_chan *prevch;
	struct fuse_session *prevse;
	fuse_processor_t proc;
	void *data;
};

#ifdef __MULTI_THREAD

static void mt_session_proc(void *data, const char *buf, size_t len,
			    struct fuse_chan *ch)
{
	struct procdata *pd = (struct procdata *) data;
	struct fuse_cmd *cmd = *(struct fuse_cmd **) buf;

	(void) len;
	(void) ch;
	pd->proc(pd->f, cmd, pd->data);
}

static void mt_session_exit(void *data, int val)
{
	struct procdata *pd = (struct procdata *) data;
	if (val)
		fuse_session_exit(pd->prevse);
	else
		fuse_session_reset(pd->prevse);
}

static int mt_session_exited(void *data)
{
	struct procdata *pd = (struct procdata *) data;
	return fuse_session_exited(pd->prevse);
}

static int mt_chan_receive(struct fuse_chan **chp, char *buf, size_t size)
{
	struct fuse_cmd *cmd;
	struct procdata *pd = (struct procdata *) fuse_chan_data(*chp);

	assert(size >= sizeof(cmd));

	cmd = fuse_read_cmd(pd->f);
	if (cmd == NULL)
		return 0;

	*(struct fuse_cmd **) buf = cmd;

	return sizeof(cmd);
}

int fuse_loop_mt_proc(struct fuse *f, fuse_processor_t proc, void *data)
{
	int res;
	struct procdata pd;
	struct fuse_session *prevse = fuse_get_session(f);
	struct fuse_session *se;
	struct fuse_chan *prevch = fuse_session_next_chan(prevse, NULL);
	struct fuse_chan *ch;
	struct fuse_session_ops sop = {
		.exit = mt_session_exit,
		.exited = mt_session_exited,
		.process = mt_session_proc,
	};
	struct fuse_chan_ops cop = {
		.receive = mt_chan_receive,
	};

	pd.f = f;
	pd.prevch = prevch;
	pd.prevse = prevse;
	pd.proc = proc;
	pd.data = data;

	se = fuse_session_new(&sop, &pd);
	if (se == NULL)
		return -1;

	ch = fuse_chan_new(&cop, fuse_chan_fd(prevch),
			   sizeof(struct fuse_cmd *), &pd);
	if (ch == NULL) {
		fuse_session_destroy(se);
		return -1;
	}
	fuse_session_add_chan(se, ch);
	res = fuse_session_loop_mt(se);
	fuse_session_destroy(se);
	return res;
}

int fuse_loop_mt(struct fuse *f)
{
	if (f == NULL)
		return -1;

	int res = fuse_start_cleanup_thread(f);
	if (res)
		return -1;

	res = fuse_session_loop_mt(fuse_get_session(f));
	fuse_stop_cleanup_thread(f);
	return res;
}

/* FUSE_SYMVER(".symver fuse_loop_mt_proc,__fuse_loop_mt@"); */

#endif
