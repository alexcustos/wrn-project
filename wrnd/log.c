// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "log.h"
#include "wrnd.h"

#define LOG_ERROR_FILE "error.log"
#define LOG_COMMON_FILE "wrnd.log"
#define LOG_WDT_FILE "device_wdt.log"
#define LOG_RNG_FILE "device_rng.log"
#define LOG_NRF_FILE "device_nrf.log"

static FILE *fp_error = NULL, *fp_common = NULL, *fp_wdt = NULL, *fp_rng = NULL, *fp_nrf = NULL;
static ino_t ino_error = 0, ino_common = 0, ino_wdt = 0, ino_rng = 0, ino_nrf = 0; 
static char message_buffer[MESSAGE_BUFFER_SIZE];
static char time_buffer[21];

static FILE *open_log(const char *fname, ino_t *ino)
{
	struct stat sb;
	FILE *fp;
	char *fpath = malloc(snprintf(NULL, 0, "%s/%s", DEFAULT_LOGDIR, fname) + 1);
	if (fpath == NULL) {
		fprintf(stderr, "Cannot allocate required memory: openlog\n");
		return NULL;
	}
	sprintf(fpath, "%s/%s", DEFAULT_LOGDIR, fname);

	if (access(fpath, F_OK) == -1) {
		fp = fopen(fpath, "a+");
		if (fp != NULL) {
			fclose(fp);
			chmod(fpath, 0640);
		}
	}

	fp = fopen(fpath, "a+");
	if (fp == NULL)
		fprintf(stderr, "Cannot create or open the log file %s: %s\n", fpath, strerror(errno));

	if (stat(fpath, &sb) == 0)
		*ino = sb.st_ino;

	free(fpath);
	return fp;
}

static FILE *reopen_log(const char *fname, FILE *fp, ino_t *ino)
{
	struct stat sb;
	char *fpath = malloc(snprintf(NULL, 0, "%s/%s", DEFAULT_LOGDIR, fname) + 1);
	if (fpath == NULL) {
		log_message(WRND_ERROR, "Cannot allocate required memory: reopenlog");
		return NULL;
	}
	sprintf(fpath, "%s/%s", DEFAULT_LOGDIR, fname);

	if (stat(fpath, &sb) != 0 || sb.st_ino != *ino)
	{
		fp = freopen(fpath, "a+", fp);
		if (fp == NULL)
			log_message(WRND_ERROR, "Cannot reopen the log file %s: %s\n", fpath, strerror(errno));
		else
			*ino = sb.st_ino;
	}

	free(fpath);
	return fp;
}

static bool write_message(FILE *fp, const char *buf)
{
	int ret;
	time_t t;
	struct tm *now;

	time(&t);
	now = localtime(&t);
	strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", now);

	ret = fprintf(fp, "%s  %s\n", time_buffer, buf);
	if(ret < 0)
		return false;
	fflush(fp);

	return true;
}

bool open_logs()
{
	if (mkdir(DEFAULT_LOGDIR, 0750) && errno != EEXIST) {
		fprintf(stderr, "Cannot create directory %s: %s\n", DEFAULT_LOGDIR, strerror(errno));
		return false;
	}
	fp_error = open_log(LOG_ERROR_FILE, &ino_error);
	fp_common = open_log(LOG_COMMON_FILE, &ino_common);
	fp_wdt = open_log(LOG_WDT_FILE, &ino_wdt);
	fp_rng = open_log(LOG_RNG_FILE, &ino_rng);
	fp_nrf = open_log(LOG_NRF_FILE, &ino_nrf);

	if (fp_error != NULL && fp_common != NULL && fp_wdt != NULL && fp_rng != NULL && fp_nrf != NULL)
		return true;

	return false;
}

void close_logs(void)
{
	fclose(fp_error); ino_error = 0;
	fclose(fp_common); ino_common = 0;
	fclose(fp_wdt); ino_wdt = 0;
	fclose(fp_rng); ino_rng = 0;
	fclose(fp_nrf); ino_nrf = 0;
}

bool reopen_logs(void)
{
	if (!reopen_log(LOG_ERROR_FILE, fp_error, &ino_error))
		return false;
	if (!reopen_log(LOG_COMMON_FILE, fp_common, &ino_common))
		return false;
	if (!reopen_log(LOG_WDT_FILE, fp_wdt, &ino_wdt))
		return false;
	if (!reopen_log(LOG_RNG_FILE, fp_rng, &ino_rng))
		return false;
	if (!reopen_log(LOG_NRF_FILE, fp_nrf, &ino_nrf))
		return false;

	return true;
}

bool wrndlog(enum log_destination dest, const char *fmt, ...)
{
	bool ret;
	va_list args;

	va_start(args, fmt);
	vsprintf(message_buffer, fmt, args);
	va_end(args);
	message_buffer[sizeof(message_buffer) - 1] = '\0';

	switch(dest) {
		case WRND_ERROR:
			ret = write_message(fp_error, message_buffer);
			break;
		case WRND_COMMON:
			ret = write_message(fp_common, message_buffer);
			break;
		case WRND_WDT:
			ret = write_message(fp_wdt, message_buffer);
			break;
		case WRND_RNG:
			ret = write_message(fp_rng, message_buffer);
			break;
		case WRND_NRF:
			ret = write_message(fp_nrf, message_buffer);
			break;
		default:
			log_message(WRND_ERROR, "Invalid log destination: %d", dest);
			return false;
	}

	return ret;
}
