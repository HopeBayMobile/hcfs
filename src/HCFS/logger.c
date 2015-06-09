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

#include "params.h"
#include "global.h"

extern SYSTEM_CONF_STRUCT system_config;

int open_log(char *filename)
{
	int ret, errcode;

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
	logptr->fptr = fopen(filename, "a+");
	setbuf(logptr->fptr, NULL);
	fchmod(fileno(logptr->fptr), 0600);

	if (logptr->fptr == NULL) {
		errcode = errno;
		write_log(0, "Failed to open log file. Code %d, %s\n",
				errcode, strerror(errcode));
		free(logptr);
		logptr = NULL;
		return -errcode;
	}

	ret = dup2(fileno(logptr->fptr), fileno(stdout));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Failed to redirect stdout. Code %d, %s\n",
				errcode, strerror(errcode));
		free(logptr);
		logptr = NULL;
		return -errcode;
	}

	ret = dup2(fileno(logptr->fptr), fileno(stderr));
	if (ret < 0) {
		errcode = errno;
		write_log(0, "Failed to redirect stderr. Code %d, %s\n",
				errcode, strerror(errcode));
		free(logptr);
		logptr = NULL;
		return -errcode;
	}

	return 0;
}

int write_log(int level, char *format, ...)
{
	va_list alist;
	struct timeval tmptime;
	struct tm tmptm;
	char add_newline;
	char timestr[100];

	va_start(alist, format);
	if (format[strlen(format)-1] != '\n')
		add_newline = TRUE;
	else
		add_newline = FALSE;
	if (logptr == NULL) {
		if (level <= LOG_LEVEL) {
			gettimeofday(&tmptime, NULL);
			localtime_r(&(tmptime.tv_sec), &tmptm);
			strftime(timestr, 90, "%F %T", &tmptm);

			printf("%s.%06d\t", timestr,
				(unsigned int)tmptime.tv_usec);

			vprintf(format, alist);
			if (add_newline == TRUE)
				printf("\n");
		}
	} else {
		if (level <= LOG_LEVEL) {
			gettimeofday(&tmptime, NULL);
			localtime_r(&(tmptime.tv_sec), &tmptm);
			strftime(timestr, 90, "%F %T", &tmptm);

			sem_wait(&(logptr->logsem));
			fprintf(logptr->fptr, "%s.%06d\t", timestr,
				(unsigned int)tmptime.tv_usec);
			vfprintf(logptr->fptr, format, alist);
			if (add_newline == TRUE)
				fprintf(logptr->fptr, "\n");

			sem_post(&(logptr->logsem));
		}
	}

	va_end(alist);
	return 0;

}

int close_log(void)
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
