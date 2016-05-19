// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include "wrnd.h"

unsigned long time_delta(struct timeval *t)
{
	struct timeval now;

	if (t == NULL)
		return 0;

	gettimeofday(&now, NULL);
    return (now.tv_sec - t->tv_sec) * 1000 + (now.tv_usec - t->tv_usec) / 1000;
}

bool check_pid_file(const char *fname)
{
    FILE *pid_fd;
	unsigned int pid;

    if (fname == NULL)
		return false;

	pid_fd = fopen(fname, "r");
	if (pid_fd != NULL) {
		pid = 0;
		if (fscanf(pid_fd, "%u", &pid) != 1)
			pid = 0;
		fclose(pid_fd);

		if (pid > 0 && kill(pid, 0) == 0) {
			log_message(WRND_ERROR, "PID file %s is already used by PID=%d", fname, pid);
			return false;
		}
	}

	return true;
}

bool write_pid_file(const char *fname)
{
    FILE *pid_fd;
    pid_t daemon_pid;

    if (fname == NULL)
		return false;

    pid_fd = fopen(fname, "w");
    if (pid_fd != NULL) {
		daemon_pid = getpid();
        fprintf(pid_fd, "%u\n", (unsigned int)daemon_pid);
        fclose(pid_fd);
    } else {
		log_message(WRND_ERROR, "Cannot open PID file %s: %s", fname, strerror(errno));
		return false;
	}

    return true;
}
