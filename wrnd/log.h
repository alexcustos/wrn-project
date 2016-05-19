// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#ifndef LOG_H_
#define LOG_H_

#include <stdbool.h>

#define MESSAGE_BUFFER_SIZE 2048
#define DEFAULT_LOGDIR "/var/log/wrnd"

enum log_destination {
	WRND_ERROR,
	WRND_COMMON,
	WRND_WDT,
	WRND_RNG,
	WRND_NRF
};

bool open_logs();
void close_logs(void);
bool reopen_logs(void);
bool wrndlog(enum log_destination, const char *, ...);  // __attribute__ ((format (printf, 2, 3)))

#endif /* LOG_H_ */
