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

	ret = _open_log_file();
	if (ret < 0) {
		errcode = errno;
		free(logptr->log_filename);
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

int32_t write_log(int32_t level, char *format, ...)
{
	va_list alist;
	struct timeval tmptime;
	struct tm tmptm;
	char add_newline;
	char timestr[100];
	int32_t this_logsize, ret;

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
	} else {
		gettimeofday(&tmptime, NULL);
		localtime_r(&(tmptime.tv_sec), &tmptm);
		strftime(timestr, 90, "%F %T", &tmptm);

		sem_wait(&(logptr->logsem));
		/* Time */
		this_logsize = fprintf(logptr->fptr, "%s.%06d\t", timestr,
			(uint32_t)tmptime.tv_usec);
		if (this_logsize > 0)
			logptr->now_log_size += this_logsize;
		/* Log message */
		this_logsize = vfprintf(logptr->fptr, format, alist);
		if (this_logsize > 0)
			logptr->now_log_size += this_logsize;
		if (add_newline == TRUE) {
			fprintf(logptr->fptr, "\n");
			logptr->now_log_size += 1;
		}

		/* Create new log file */
		if (logptr->now_log_size >= MAX_LOG_SIZE) {
			fprintf(logptr->fptr, "This log size: %d\n",
					logptr->now_log_size);
			fclose(logptr->fptr);
			logptr->fptr = NULL;
			_rename_logfile();
			ret = _open_log_file();
			if (ret < 0) {
				free(logptr->log_filename);
				free(logptr);
				logptr = NULL;
			}
		}

		sem_post(&(logptr->logsem));
	}

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
	free(logptr);
	logptr = NULL;
	return 0;
}
