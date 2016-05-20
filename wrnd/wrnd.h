// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#ifndef WRND_H_
#define WRND_H_

#include <syslog.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include "log.h"

#define MAJOR_VERSION 0
#define MINOR_VERSION 1

#define DEFAULT_PIDDIR "/run/wrnd"
#define SERIAL_RX_BUFFER_SIZE 1024  // may came from WDT:LOG
#define SERIAL_RX_SYNC_SEQUENCE 3
#define SERIAL_SYNC_TIMEOUT 2000  // ms
#define SERIAL_SYNC_RETRY 3
#define MAX_VERBOSE_LEVEL 3

#define LOG_EMERG   0   // system is unusable
#define LOG_ALERT   1   // action must be taken immediately
#define LOG_CRIT    2   // critical conditions
#define LOG_ERR     3   // error conditions
#define LOG_WARNING 4   // warning conditions
#define LOG_NOTICE  5   // normal but significant condition
#define LOG_INFO    6   // informational
#define LOG_DEBUG   7   // debug-level messages

enum transmission_status {
	TX_HEADER,
	TX_PAYLOAD,
	TX_SYNC,
	TX_UNKNOWN
};

enum verbose_level {
	VERBOSE_L0 = 0,
	VERBOSE_L1,
	VERBOSE_L2,
	VERBOSE_L3
};

struct arguments {
	char *device_port;
	unsigned int baud_rate;
	unsigned char vtime;
	char *rng_fifo;
	char *nrf_fifo;
	char *wdt_fifo;
	char *pid_file;
    unsigned char verbose;
    bool daemonize;
};

extern struct arguments *arguments;
extern int serial_fd;

#define log_message(destination, fmt, args...) do { \
	if (arguments->daemonize) { \
		wrndlog((destination), fmt, ##args); \
	} else { \
		fprintf(stderr, "[%u] ", destination); \
		fprintf(stderr, fmt, ##args); \
		fprintf(stderr, "\n"); \
	} \
} while (0)

#endif /* WRND_H_ */
