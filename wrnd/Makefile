# Copyright (c) 2016 Aleksandr Borisenko
# Distributed under the terms of the GNU General Public License v2

.PHONY: install debug clean ins rm

TARGET_DAEMON = wrnd
TARGET_WDT = wrn_wdt
PREFIX = /usr/local

ifneq ($(KERNELRELEASE),)
# call from kernel build system
obj-m := $(TARGET_WDT).o

else
CC = gcc
KERNEL := $(shell uname -r)
INSTALL := $(shell which install)
# -Wall Turns on most, but not all, compiler warnings
CFLAGS = -Wall -std=gnu11 -pthread

ifeq ($(BUILD),debug)
# -g Adds debugging information to the executable file
CFLAGS += -g -O0 -DDEBUG

else
# -s Remove all symbol table and relocation information from the executable
CFLAGS += -s -O2 -DNDEBUG

endif
# ifneq BUILD

all: daemon driver

debug:
	$(MAKE) daemon BUILD=debug

daemon: $(TARGET_DAEMON).o serialport.o log.o devices.o utils.o
	$(CC) $(CFLAGS) -o $(TARGET_DAEMON) $(TARGET_DAEMON).o serialport.o log.o devices.o utils.o

$(TARGET_DAEMON).o: $(TARGET_DAEMON).c $(TARGET_DAEMON).h
	$(CC) $(CFLAGS) -c $(TARGET_DAEMON).c

serialport.o: serialport.c serialport.h
	$(CC) $(CFLAGS) -c serialport.c

log.o: log.c log.h
	$(CC) $(CFLAGS) -c log.c

devices.o: devices.c devices.h
	$(CC) $(CFLAGS) -c devices.c

utils.o: utils.c utils.h
	$(CC) $(CFLAGS) -c utils.c

driver:
	$(MAKE) -C /lib/modules/$(KERNEL)/build M=$(PWD)

install: daemon driver
	$(INSTALL) -m 755 -o root -g root ./wrnd $(PREFIX)/bin
	$(INSTALL) -m 755 -o root -g root ./gentoo/wrnctrl $(PREFIX)/bin
	$(INSTALL) -m 755 -o root -g root -T ./gentoo/wrnd.init.d /etc/init.d/wrnd
	$(INSTALL) -m 644 -o root -g root -T ./gentoo/wrnd.conf.d /etc/conf.d/wrnd
	$(INSTALL) -m 755 -o root -g root -T ./gentoo/wrnd.log.daily /etc/cron.daily/wrnd
	$(INSTALL) -m 644 -o root -g root -T ./gentoo/wrnd.logrotate.d /etc/logrotate.d/wrnd
	$(MAKE) -C /lib/modules/$(KERNEL)/build M=$(PWD) modules_install

clean:
	$(RM) -rf .tmp_versions
	$(RM) -f $(TARGET_DAEMON) *.o *.ko *.tmp *.mod.c .*.cmd *.symvers *.order

ins: driver rm
	insmod $(TARGET_WDT).ko

rm:
	- rmmod $(TARGET_WDT)

endif
# ifneq KERNELRELEASE
