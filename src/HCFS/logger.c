/*************************************************************************
*
* Copyright © 2015 Hope Bay Technologies, Inc. All rights reserved.
*
* File Name: logger.c
* Abstract: The c source file for logger functions
*
* Revision History
* 2015/6/1 Jiahong created this file.
*
**************************************************************************/

#include "logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <semaphore.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>
#include <unistd.h>

#include "params.h"
#include "global.h"
#include "utils.h"
#include "fuseop.h"

struct LOG_internal {
        sem_t logsem;
        FILE *fptr;
        int32_t now_log_size;
        char *log_filename;
        char *latest_log_msg;
        char *now_log_msg;
        int32_t repeated_times;
        struct timeval latest_log_time;
        time_t latest_log_start_time;
        pthread_t tid;
        pthread_attr_t flusher_attr;
        BOOL flusher_is_created;
};

/**
 * Open/create the log file named "log_filename" and initialize some
 * log file info, such as "now_log_size", file mode.
 */
static int32_t _open_log_file(void)
{
	int32_t ret, errcode;
	char log_file[500];
	struct stat logstat; /* raw file ops */

	logptr->now_log_size = 0;
	if (LOG_PATH != NULL)
		sprintf(log_file, "%s/%s", LOG_PATH, logptr->log_filename);
	else
		sprintf(log_file, "%s", logptr->log_filename);
	logptr->fptr = fopen(log_file, "a+");
	if (logptr->fptr == NULL) {
		errcode = errno;
		fprintf(stderr, "Failed to open log file. Code %d, %s\n",
				errcode, strerror(errcode));
		return -errcode;
	}
	setbuf(logptr->fptr, NULL);
	/* Change the log level so that the log can be deleted */
	fchmod(fileno(logptr->fptr), 0666);
	fstat(fileno(logptr->fptr), &logstat);
	logptr->now_log_size = logstat.st_size;

	ret = dup2(fileno(logptr->fptr), fileno(stdout));
	if (ret < 0) {
		errcode = errno;
		fprintf(stderr, "Failed to redirect stdout. Code %d, %s\n",
				errcode, strerror(errcode));
		return -errcode;
	}

	ret = dup2(fileno(logptr->fptr), fileno(stderr));
	if (ret < 0) {
		errcode = errno;
		fprintf(stderr, "Failed to redirect stderr. Code %d, %s\n",
				errcode, strerror(errcode));
		return -errcode;
	}

	return 0;
}

int32_t open_log(char *filename)
{
	int32_t ret, errcode;

	if (logptr != NULL) {
		write_log(0, "Attempted to open log file twice. Aborting.\n");
		return -EPERM;
	}
	logptr = malloc(sizeof(LOG_STRUCT));
	if (logptr == NULL) {
		write_log(0, "Opening log failed (out of memory).\n");
		return -ENOMEM;
	}
	memset(logptr, 0, sizeof(LOG_STRUCT));

	ret = sem_init(&(logptr->logsem), 0, 1);
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Failed to initialize logger. Code %d, %s\n",
				errcode, strerror(errcode));
		free(logptr);
		logptr = NULL;
		return -errcode;
	}

	logptr->log_filename = (char *) malloc(strlen(filename) + 2);
	strcpy(logptr->log_filename, filename);

	logptr->latest_log_msg = (char *) calloc(LOG_MSG_SIZE, 1);
	logptr->now_log_msg = (char *) calloc(LOG_MSG_SIZE, 1);

	pthread_attr_init(&(logptr->flusher_attr));
	pthread_attr_setdetachstate(&(logptr->flusher_attr),
			PTHREAD_CREATE_DETACHED);

	ret = _open_log_file();
	if (ret < 0) {
		errcode = errno;
		free(logptr->log_filename);
		free(logptr->now_log_msg);
		free(logptr->latest_log_msg);
		free(logptr);
		logptr = NULL;
		return -errcode;
	}

#ifdef VERSION_NUM
	write_log(2, "\nVersion: %s", VERSION_NUM);
#endif
	write_log(2, "\nStart logging %s\n", filename);
	return 0;
}

/**
 * Shift log files by renaming the file name. Latest log file is <file name>.1,
 * and Oldest one is <file name>.5.
 */
static void _rename_logfile(void)
{
	int32_t log_idx, miss_log_idx;
	char base_log_path[400], log_path[500], prev_log_path[500];

	if (LOG_PATH != NULL)
		sprintf(base_log_path, "%s/%s", LOG_PATH, logptr->log_filename);
	else
		sprintf(base_log_path, "%s", logptr->log_filename);

	/* Find first missing log file */
	for (log_idx = 1; log_idx <= NUM_LOG_FILE; log_idx++) {
		sprintf(log_path, "%s.%d", base_log_path, log_idx);
		if (access(log_path, F_OK) < 0)
			break;
	}
	miss_log_idx = log_idx;
	if (miss_log_idx > NUM_LOG_FILE) {
		sprintf(log_path, "%s.%d", base_log_path, NUM_LOG_FILE);
		unlink(log_path);
		miss_log_idx = NUM_LOG_FILE;
	}

	/* Shift */
	for (log_idx = miss_log_idx; log_idx > 0; log_idx--) {
		sprintf(log_path, "%s.%d", base_log_path, log_idx);
		if (log_idx == 1)
			sprintf(prev_log_path, "%s", base_log_path);
		else
			sprintf(prev_log_path, "%s.%d", base_log_path,
					log_idx - 1);
		rename(prev_log_path, log_path);
	}
}

static inline void _write_repeated_log(void)
{
	int32_t log_size;
	struct tm tmptm;
	char timestr[100];
	uint32_t usec;

	localtime_r(&(logptr->latest_log_time.tv_sec), &tmptm);
	strftime(timestr, 90, "%F %T", &tmptm);
	/* Remove newline character if it is last character in string */
	log_size = strlen(logptr->latest_log_msg);
	if (logptr->latest_log_msg[log_size - 1] == '\n')
		logptr->latest_log_msg[log_size - 1] = '\0';

	/* Write log */
	usec = logptr->latest_log_time.tv_usec;
	if (logptr->repeated_times > 1)
		log_size = fprintf(logptr->fptr, "%s.%06d\t"
			"%s [repeat %d times]\n", timestr, usec,
			logptr->latest_log_msg,
			logptr->repeated_times);
	else
		log_size = fprintf(logptr->fptr, "%s.%06d\t"
			"%s\n", timestr, usec,
			logptr->latest_log_msg);

	if (log_size > 0)
		logptr->now_log_size += log_size;

	logptr->latest_log_time.tv_sec = 0;
	logptr->latest_log_time.tv_usec = 0;
	logptr->latest_log_start_time = 0;
	logptr->repeated_times = 0;
	logptr->latest_log_msg[0] = '\0';

	return;
}

static inline void _check_log_file_size(void)
{
	if (logptr->now_log_size >= MAX_LOG_FILE_SIZE) {
		fclose(logptr->fptr);
		logptr->fptr = NULL;
		_rename_logfile();
		if (_open_log_file() < 0) {
			free(logptr->log_filename);
			free(logptr->now_log_msg);
			free(logptr->latest_log_msg);
			free(logptr);
			logptr = NULL;
		}
	}
}

#define REPEATED_LOG_IS_CACHED() (logptr->latest_log_start_time > 0)

static void log_sweeper(void)
{
	int32_t sleep_sec, timediff;
	struct timeval tmptime;

	sleep_sec = 0;

	while (1) {
		sleep(sleep_sec);
		if (!logptr)
			return;

		gettimeofday(&tmptime, NULL);
		sem_wait(&(logptr->logsem));
		/* Leave if no log was cached */
		if (!REPEATED_LOG_IS_CACHED())
			break;

		timediff = tmptime.tv_sec -
			logptr->latest_log_start_time;
		if (logptr->repeated_times > 0) {
			/* Flush immediately if system is going to shutdown */
			if (hcfs_system->system_going_down == FALSE) {
				if (timediff < FLUSH_TIME_INTERVAL) {
					sleep_sec = FLUSH_TIME_INTERVAL -
							timediff;
					sem_post(&(logptr->logsem));
					continue;
				}
			}

			/* When timediff >= FLUSH_TIME_INTERVAL, flush
			 * the log and leave loop */
			_write_repeated_log();

		} else {
			/* If repeate times == 0, check timediff and
			 * reset cached log. */
			if (timediff >= FLUSH_TIME_INTERVAL) {
				logptr->latest_log_time.tv_sec = 0;
				logptr->latest_log_time.tv_usec = 0;
				logptr->latest_log_start_time = 0;
				logptr->repeated_times = 0;
				logptr->latest_log_msg[0] = '\0';
			}
		}

		break;
	}
	logptr->flusher_is_created = FALSE;
	sem_post(&(logptr->logsem));
	return;
}

/**
 * Write log message. If LOG_COMPRESS is enable, the log msg will be deffered
 * to write until 3 secs since the first repeated log msg appearance time. That
 * is, the repeated log msg will be flushed every 3 secs. Beside, the latest
 * log file is <log file name>, on the other hand the oldest one is
 * <log file name>.<NUM_LOG_FILE>. E.g.: hcfs_android_log, hcfs_android_log.1,
 * hcfs_android_log.2...hcfs_android_log.5. Oldest log file will be removed
 * while number of log file is more than NUM_LOG_FILE.
 *
 * @return 0 on success, otherwise negative error code.
 */
int32_t write_log(int32_t level, const char *format, ...)
{
	va_list alist;
	struct timeval tmptime;
	struct tm tmptm;
	char add_newline;
	char timestr[100];
	int32_t this_logsize, ret_size;
	char *temp_ptr;

	if (system_config && level > LOG_LEVEL)
		return 0;

	va_start(alist, format);
	if (format[strlen(format)-1] != '\n')
		add_newline = TRUE;
	else
		add_newline = FALSE;
	if ((logptr == NULL) || (logptr->fptr == NULL)) {
		gettimeofday(&tmptime, NULL);
		localtime_r(&(tmptime.tv_sec), &tmptm);
		strftime(timestr, 90, "%F %T", &tmptm);

		printf("%s.%06d\t", timestr,
			(uint32_t)tmptime.tv_usec);

		vprintf(format, alist);
		if (add_newline == TRUE)
			printf("\n");

		va_end(alist);
		return 0;
	} 

	/* Write log msg */	
	gettimeofday(&tmptime, NULL);
	localtime_r(&(tmptime.tv_sec), &tmptm);
	strftime(timestr, 90, "%F %T", &tmptm);

	sem_wait(&(logptr->logsem));
	if (LOG_COMPRESS == FALSE) { /* Write when log compression is closed */
		this_logsize = fprintf(logptr->fptr, "%s.%06d\t",
				timestr, (uint32_t)tmptime.tv_usec);
		logptr->now_log_size += this_logsize;
		this_logsize = vfprintf(logptr->fptr, format, alist);
		logptr->now_log_size += this_logsize;
		if (add_newline == TRUE) {
			fprintf(logptr->fptr, "\n");
			logptr->now_log_size += 1;
		}

		_check_log_file_size();
		sem_post(&(logptr->logsem));
		va_end(alist);
		return 0;
	}

	ret_size = vsnprintf(logptr->now_log_msg,
			LOG_MSG_SIZE, format, alist);
	/* Derectly write to file when msg is too long */
	if (ret_size >= LOG_MSG_SIZE) {
		if (REPEATED_LOG_IS_CACHED()) {
			if (logptr->repeated_times > 0) {
				_write_repeated_log();
			} else {
				logptr->latest_log_time.tv_sec = 0;
				logptr->latest_log_time.tv_usec = 0;
				logptr->latest_log_start_time = 0;
				logptr->repeated_times = 0;
				logptr->latest_log_msg[0] = '\0';
			}
		}
		va_start(alist, format);
		this_logsize = fprintf(logptr->fptr, "%s.%06d\t",
			timestr, (uint32_t)tmptime.tv_usec);
		logptr->now_log_size += this_logsize;
		this_logsize = vfprintf(logptr->fptr, format, alist);
		logptr->now_log_size += this_logsize;
		if (add_newline == TRUE) {
			fprintf(logptr->fptr, "\n");
			logptr->now_log_size += 1;
		}

		_check_log_file_size();
		sem_post(&(logptr->logsem));
		va_end(alist);
		return 0;
	}

	/* Compare between new log and latest log */
	if (!strcmp(logptr->now_log_msg, logptr->latest_log_msg)) {
		logptr->latest_log_time = tmptime;
		logptr->repeated_times++;
		if (logptr->flusher_is_created == FALSE) {
			pthread_create(&(logptr->tid), &(logptr->flusher_attr),
				(void *)log_sweeper, NULL);
			logptr->flusher_is_created = TRUE;
		}
		sem_post(&(logptr->logsem));
		va_end(alist);
		return 0;
	}

	/* Check if the repeated log msg should be printed */
	if (logptr->repeated_times > 0)
		_write_repeated_log();

	/* Time and log msg */
	this_logsize = fprintf(logptr->fptr, "%s.%06d\t%s",
			timestr, (uint32_t)tmptime.tv_usec,
			logptr->now_log_msg);
	if (this_logsize > 0)
		logptr->now_log_size += this_logsize;
	if (add_newline == TRUE) {
		fprintf(logptr->fptr, "\n");
		logptr->now_log_size += 1;
	}

	/* Let now log be the latest log */
	temp_ptr = logptr->now_log_msg;
	logptr->now_log_msg = logptr->latest_log_msg;
	logptr->latest_log_msg = temp_ptr;
	logptr->latest_log_time = tmptime;
	logptr->latest_log_start_time = tmptime.tv_sec;
	logptr->repeated_times = 0;

	/* Check log file size */
	_check_log_file_size();
	sem_post(&(logptr->logsem));
	va_end(alist);
	return 0;
}

int32_t close_log(void)
{
	struct timespec time_to_sleep;

	time_to_sleep.tv_sec = 0;
	time_to_sleep.tv_nsec = 99999999; /*0.1 sec sleep*/

	if (logptr == NULL)
		return 0;

	while (logptr->flusher_is_created)
		nanosleep(&time_to_sleep, NULL);

	if (logptr->fptr != NULL) {
		fclose(logptr->fptr);
		logptr->fptr = NULL;
	}
	sem_destroy(&(logptr->logsem));
	free(logptr->log_filename);
	free(logptr->now_log_msg);
	free(logptr->latest_log_msg);
	free(logptr);
	logptr = NULL;
	return 0;
}

#ifdef UNITTEST
FILE *logger_get_fileptr(LOG_STRUCT *ptr)
{
	if (ptr)
		return ptr->fptr;
	return NULL;
}

sem_t *logger_get_semaphore(LOG_STRUCT *ptr)
{
	if (ptr)
		return &(ptr->logsem);
	return NULL;
}
#endif  /* UNITTEST */
