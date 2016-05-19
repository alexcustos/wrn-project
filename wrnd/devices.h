// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#ifndef DEVICES_H_
#define DEVICES_H_

#include <inttypes.h>
#include <stdbool.h>

#define MAX_SYNC_SEQUENCE 8
#define COMMAND_FIFO "/run/wrnd/cmd.fifo"
#define COMMAND_FEEDBACK_SIZE 2024


enum command_type {
	CMD_COMMON = 0,
	CMD_WDT,
	CMD_RNG,
	CMD_RNG_SEND,
	CMD_NRF,
	CMD_NRF_FORWARD,
	CMD_UNKNOWN
};

enum common_command {
	COMMON_SYNC = 0,
	COMMON_TIME,
	COMMON_STATUS,
	COMMON_RESET,
	COMMON_PROGRAM,
	COMMON_LOG_CLEAN,
	COMMON_UNKNOWN
};

enum wdt_command {
	WDT_KEEP_ALIVE = 0,
	WDT_DEACTIVATE,
	WDT_STATUS,
	WDT_TIMEOUT,
	WDT_LOG,
	WDT_UNKNOWN
};

enum rng_command {
	RNG_FLOOD_ON = 0,
	RNG_FLOOD_OFF,
	RNG_STATUS,
	RNG_UNKNOWN
};

enum nrf_forward_command {
	NRF_FORWARD_L = 0,
	NRF_FORWARD_UNKNOWN
};

enum destination_fifo {
	FIFO_CMD,
	FIFO_RNG,
	FIFO_NRF
};

enum log_event {
	LOG_EMPTY = 0,
	LOG_BOOT,
	LOG_RESET
};

struct payload_header {
	uint8_t type_id;
	uint8_t cmd_id;
	uint16_t seq_num;
	int16_t payload_size;
} __attribute__ ((__packed__));

struct common_status
{
	int32_t time;
	uint32_t uptime; // sec
	int32_t vcc;
	uint8_t nlock;
} __attribute__ ((__packed__));

struct wdt_status
{
	uint8_t active;
	uint16_t timeout;
	uint16_t min_delta;
	uint16_t log_length;
} __attribute__ ((__packed__));

struct rng_status
{
	uint8_t threshold;
	uint8_t calibrated;
	uint8_t flood;
	uint16_t fault;
} __attribute__ ((__packed__));

struct nrf_light
{
	uint16_t id;
	uint32_t uptime;
	uint8_t light;
	int32_t vcc;
	int32_t tmp36;
	uint8_t stat;
} __attribute__ ((__packed__));

struct log_record
{
	int32_t time;
	uint8_t log_event;
} __attribute__ ((__packed__));


bool init_fifos();
bool init_device(int);
void close_device(int);


bool device_write_command(int, const char *, const char *);
bool device_update_time(int);
bool device_send_sync(int, unsigned int);

void log_device_header(struct payload_header *);
void log_device_error(struct payload_header *);
void log_device_payload(struct payload_header *, const unsigned char *);

bool write_fifo(enum destination_fifo, const char *, size_t);
bool write_fifo_and_close(enum destination_fifo, const char *, size_t, bool);

void process_payload(struct payload_header *, const unsigned char *);
void process_confirmation(struct payload_header *);

#endif /* DEVICES_H_ */
