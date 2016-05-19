// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#ifndef UTILS_H_
#define UTILS_H_

#include <sys/types.h>


unsigned long time_delta(struct timeval *);
bool check_pid_file(const char *);
bool write_pid_file(const char *);

#endif /* UTILS_H_ */
