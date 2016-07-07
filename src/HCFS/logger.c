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

/**
 * Open/create the log file named "log_filename" and initialize some
 * log file info, such as "now_log_size", file mode.
 */
int32_t _open_log_file()
{
	int32_t ret, errcode;
	char log_file[500];
	struct stat logstat;

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

	logptr->latest_log_msg = (char *) malloc(LOG_MSG_SIZE);
	logptr->now_log_msg = (char *) malloc(LOG_MSG_SIZE);

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

	return 0;
}

/**
 * Shift log files by renaming the file name. Latest log file is <file name>.1,
 * and Oldest one is <file name>.5.
 */
void _rename_logfile()
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

static inline void _write_repeated_log(char *timestr, uint32_t usec)
{
	int32_t log_size;

	/* Remove newline character if it is last character in string */
	log_size = strlen(logptr->latest_log_msg);
	if (logptr->latest_log_msg[log_size - 1] == '\n')
		logptr->latest_log_msg[log_size - 1] = '\0';

	/* Write log */
	log_size = fprintf(logptr->fptr, "%s.%06d\t"
			"%s [repeat %d times]\n", timestr, usec,
			logptr->latest_log_msg,
			logptr->repeated_times);
	if (log_size > 0)
		logptr->now_log_size += log_size;

	logptr->latest_log_sec = 0;
	logptr->repeated_times = 0;
	logptr->latest_log_msg[0] = '\0';

	return;
}

static inline void _check_log_file_size()
{
	if (logptr->now_log_size >= MAX_LOG_SIZE) {
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
int32_t write_log(int32_t level, char *format, ...)
{
	va_list alist;
	struct timeval tmptime;
	struct tm tmptm;
	char add_newline;
	char timestr[100];
	int32_t this_logsize, ret_size;
	char *temp_ptr;

	if (logptr && (logptr->latest_log_sec > 0)) {
		gettimeofday(&tmptime, NULL);
		/* Flush repeated log msg every FLUSH_TIME_INTERVAL secs */
		if (tmptime.tv_sec - logptr->latest_log_sec >=
				FLUSH_TIME_INTERVAL) {
			if (logptr->repeated_times > 0) {
				localtime_r(&(tmptime.tv_sec), &tmptm);
				strftime(timestr, 90, "%F %T", &tmptm);
				_write_repeated_log(timestr,
					(uint32_t)tmptime.tv_usec);
			} else {
				logptr->latest_log_sec = 0;
				logptr->repeated_times = 0;
				logptr->latest_log_msg[0] = '\0';
			}
		}
	}

	if (level > LOG_LEVEL)
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
	if (LOG_COMPRESS == FALSE) {
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
		logptr->latest_log_sec = 0;
		logptr->repeated_times = 0;
		logptr->latest_log_msg[0] = '\0';
		sem_post(&(logptr->logsem));
		va_end(alist);
		return 0;
	}

	if (!strcmp(logptr->now_log_msg, logptr->latest_log_msg)) {
		logptr->repeated_times++;
		sem_post(&(logptr->logsem));
		va_end(alist);
		return 0;
	}

	/* Check if the repeated log msg should be printed */
	if (logptr->repeated_times > 0)
		_write_repeated_log(timestr, (uint32_t)tmptime.tv_usec);

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

	/* Let now log be latest log */
	temp_ptr = logptr->now_log_msg;
	logptr->now_log_msg = logptr->latest_log_msg;
	logptr->latest_log_msg = temp_ptr;
	logptr->latest_log_sec = tmptime.tv_sec;

	/* Check log file size */
	_check_log_file_size();
	sem_post(&(logptr->logsem));
	va_end(alist);
	return 0;
}

int32_t close_log(void)
{
	if (logptr == NULL)
		return 0;

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
