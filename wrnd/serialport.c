// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#include <stdio.h>
#include <fcntl.h>
#include <asm/termios.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include "serialport.h"
#include "wrnd.h"

extern int ioctl(int __fd, unsigned long int __request, ...); // __THROW;

static bool serialport_attributes(int fd, unsigned int speed, unsigned char vmin, unsigned char vtime)
{
	struct termios2 ttyopts;
	memset(&ttyopts, 0, sizeof ttyopts);
	if (ioctl(fd, TCGETS2, &ttyopts) != 0) {
		log_message(WRND_ERROR, "Cannot get serial port attributes: %s", strerror(errno));
		return false;
	}

	// port speed
    ttyopts.c_cflag &= ~CBAUD;
    ttyopts.c_cflag |= BOTHER;
    ttyopts.c_ispeed = speed;
    ttyopts.c_ospeed = speed;

	// no parity (8N1)
	ttyopts.c_cflag &= ~(PARENB | CSTOPB | CSIZE);
	ttyopts.c_cflag |= CS8;

    // no flow control
    ttyopts.c_cflag &= ~CRTSCTS;

	// turn on READ & ignore ctrl lines
	ttyopts.c_cflag |= (CREAD | CLOCAL);

	// turn off software flow ctrl
	ttyopts.c_iflag &= ~(IXON | IXOFF | IXANY);

	// make raw
	ttyopts.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	ttyopts.c_iflag &= ~(INLCR | ICRNL);
	ttyopts.c_oflag &= ~OPOST;

	// blocking
	ttyopts.c_cc[VMIN] = vmin;
	ttyopts.c_cc[VTIME] = vtime;

	if (ioctl(fd, TCSETS2, &ttyopts) != 0) {
		log_message(WRND_ERROR, "Failed to set serial port attributes: %s", strerror(errno));
		return false;
	}

	return true;
}

int serialport_init(const char* serialport, unsigned int speed, unsigned char vtime)
{
	int fd = open(serialport, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd == -1) {
		log_message(WRND_ERROR, "Unable to open port: %s", serialport);
		return -1;
	}

	if (serialport_attributes(fd, speed, 0, vtime))
		return fd;
	else
		close(fd);

	return -1;
}
