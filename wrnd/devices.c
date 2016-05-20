// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include "devices.h"
#include "utils.h"
#include "wrnd.h"

static int cmd_fifo_fd = -1, rng_fifo_fd = -1, nrf_fifo_fd = -1, wdt_fifo_fd = -1;
static char message_buffer[COMMAND_FEEDBACK_SIZE];
static char time_buffer[21];

static pthread_spinlock_t wrn_wdt_lock;
static pthread_t wrn_wdt_thread;
static bool wrn_wdt_nowayout = WDT_NOWAYOUT;
static struct timeval wrn_wdt_keep_alive_sent = {.tv_sec = 0, .tv_usec = 0};
static bool wrn_wdt_ok_to_close = false;


static const char **command_list[] = {
	(const char *[]){"COMMON", "SYNC", "TIME", "STATUS", "RESET", "PROGRAM", "LOG-CLEAN", "UNKNOWN", NULL},
	(const char *[]){"WDT", "KEEP-ALIVE", "DEACTIVATE", "STATUS", "TIMEOUT", "LOG", "UNKNOWN", NULL},
	(const char *[]){"RNG", "FLOOD-ON", "FLOOD-OFF", "STATUS", "UNKNOWN", NULL},
	(const char *[]){"RNG-SEND", "PAYLOAD", "UNKNOWN", NULL},
	(const char *[]){"NRF", "UNKNOWN", NULL},
	(const char *[]){"NRF-FORWARD", "L", "UNKNOWN", NULL}
};
static const char *log_event_list[] = {"EMPTY", "BOOT", "RESET"};


static const char *get_device_name(struct payload_header *header)
{
	unsigned int num_devices;

	if (header == NULL)
		return NULL;

	num_devices = sizeof(command_list) / sizeof(*command_list);
	if (header->type_id >= num_devices)
		return NULL;

	return command_list[header->type_id][0];
}

static const char *get_command_name(struct payload_header *header)
{
	size_t i;

	if (get_device_name(header) == NULL)
		return NULL;

    for (i = 0; command_list[header->type_id][i] != NULL && i <= header->cmd_id; i++);

	if ((header->cmd_id + 1) == i)
		return command_list[header->type_id][i];

	return NULL;
}

void log_device_error(struct payload_header *header)
{
	const char *dev_name, *cmd_name;

	if (header == NULL || header->payload_size >= 0)
		return;

	dev_name = get_device_name(header);
	cmd_name = get_command_name(header);

	log_message(WRND_ERROR, "Error status received from the device [%" PRIu16 "] %s:%s",
		header->seq_num, dev_name != NULL ? dev_name : "UNEXPECTED", 
		cmd_name != NULL ? cmd_name : "UNEXPECTED");
}

static void log_header(enum command_type type, const char *msg, struct payload_header *header)
{
	const char *dev_name, *cmd_name;

	size_t size = sizeof(struct payload_header);
	unsigned char *bytes = (unsigned char *)header;
	char *header_hex = malloc(size * 3 + 1);
	for (size_t i = 0; i < size; i++) {
		sprintf(header_hex + i * 3, " %02X", *(bytes + i));
	}

	dev_name = get_device_name(header);
	cmd_name = get_command_name(header);

	log_message(type, "%s:%" PRId16 " [%" PRIu16 ":%s:%s]%s", msg, header->payload_size, 
		header->seq_num, dev_name != NULL ? dev_name : "UNEXPECTED", 
		cmd_name != NULL ? cmd_name : "UNEXPECTED", header_hex);

	free(header_hex);
}

void log_device_header(struct payload_header *header)
{
	if (header == NULL)
		return;

	char *msg;
	if (header->payload_size == 0)
		msg = "Confirmation";
	else if (header->payload_size > 0)
		msg = "Payload";
	else
		msg = "Header";

	switch ((enum command_type)header->type_id) {
		case CMD_COMMON:
			log_header(WRND_COMMON, msg, header);
			break;
		case CMD_WDT:
			log_header(WRND_WDT, msg, header);
			break;
		case CMD_RNG:
		case CMD_RNG_SEND:
			log_header(WRND_RNG, msg, header);
			break;
		case CMD_NRF:
		case CMD_NRF_FORWARD:
			log_header(WRND_NRF, msg, header);
			break;
		default:
			break;
	}
}

void log_device_payload(struct payload_header *header, const unsigned char *payload)
{
	if (header == NULL || payload == NULL || header->payload_size <= 0)
		return;

	char *payload_hex = malloc(header->payload_size * 3 + 1);
	if (payload_hex == NULL) {
		log_message(WRND_ERROR, "Cannot allocate required memory: log_device_payload");
		return;
	}
	for (size_t i = 0; i < header->payload_size; i++) {
		sprintf(payload_hex + i * 3, " %02X", *(payload + i));
	}

	switch ((enum command_type)header->type_id) {
		case CMD_COMMON:
			log_message(WRND_COMMON, payload_hex + 1);
			break;
		case CMD_WDT:
			log_message(WRND_WDT, payload_hex + 1);
			break;
		case CMD_RNG:
		case CMD_RNG_SEND:
			log_message(WRND_RNG, payload_hex + 1);
			break;
		case CMD_NRF:
		case CMD_NRF_FORWARD:
			log_message(WRND_NRF, payload_hex + 1);
			break;
		default:
			break;
	}

	free(payload_hex);
}

bool device_write_command(const char *cmd, const char *name)
{
	int len, n;

	if (serial_fd == -1 || cmd == NULL || name == NULL)
		return false;

	len = strlen(cmd);
	n = write(serial_fd, cmd, len);
	n += write(serial_fd, "\n", 1);

	len++;
	if (n != len) {
		log_message(WRND_ERROR, "Cannot send command %s to the device: %d:[%d]", name, n, len);
		return false;
	}

	return true;
}

bool device_update_time()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	char *cmd = malloc(snprintf(NULL, 0, "C1:%lld", (long long)now.tv_sec) + 1);
	if (cmd == NULL) {
		log_message(WRND_ERROR, "Cannot allocate required memory: device_update_time");
		return false;
	}
	sprintf(cmd, "C1:%lld", (long long)now.tv_sec);
	if (!device_write_command(cmd, "COMMON:TIME")) {
		free(cmd);
		return false;
	}

	free(cmd);
	return true;
}

bool device_send_sync(unsigned int sequence_len)
{
	if ( sequence_len == 0 || sequence_len > MAX_SYNC_SEQUENCE)
		return false;

	char *cmd = malloc(snprintf(NULL, 0, "C0:%d", sequence_len) + 1);
	if (cmd == NULL) {
		log_message(WRND_ERROR, "Cannot allocate required memory: device_send_sync");
		return false;
	}
	sprintf(cmd, "C0:%d", sequence_len);
	if (!device_write_command(cmd, "COMMON:SYNC")) {
		free(cmd);
		return false;
	}

	free(cmd);
	return true;
}

static bool create_fifo(const char *fname, mode_t mode)
{
	if (fname == NULL)
		return false;

	if (access(fname, F_OK) == -1) {
		if (mkfifo(fname, mode) == -1) {
			log_message(WRND_ERROR, "Cannot create FIFO %s: %s", fname, strerror(errno));
			return false;
		}
	}

	return true;
}

bool init_device()
{
	if (!device_update_time())
		return false;

	if (!device_write_command("R0", "RNG:FLOOD-ON"))
		return false;

	return true;
}

bool init_fifos()
{
	if (!create_fifo(arguments->rng_fifo, 0640))
		return false;

	if (!create_fifo(arguments->nrf_fifo, 0640))
		return false;

	if (!create_fifo(arguments->wdt_fifo, 0640))
		return false;

	if (!create_fifo(COMMAND_FIFO, 0644))
		return false;	

	return true;
}

void close_device()
{
	device_write_command("R1", "RNG:FLOOD-OFF");
	close(rng_fifo_fd); rng_fifo_fd = -1;
	close(nrf_fifo_fd); nrf_fifo_fd = -1;
	close(wdt_fifo_fd); wdt_fifo_fd = -1;
	close(cmd_fifo_fd); cmd_fifo_fd = -1;
}

bool write_fifo(enum destination_fifo dest, const char *msg, size_t count)
{
	return write_fifo_and_close(dest, msg, count, false);
}

bool write_fifo_and_close(enum destination_fifo dest, const char *msg, size_t count, bool close_fifo)
{
	int *fd;
	char *fifo;

	if (msg == NULL || count <= 0)
		return false;

	switch(dest) {
		case FIFO_CMD:
			fd = &cmd_fifo_fd;
			fifo = COMMAND_FIFO;
			break;
		case FIFO_RNG:
			fd = &rng_fifo_fd;
			fifo = arguments->rng_fifo;
			break;
		case FIFO_NRF:
			fd = &nrf_fifo_fd;
			fifo = arguments->nrf_fifo;
			break;
		default:
			fd = NULL;
			break;
	}
	if (fd == NULL)
		return false;

	if (*fd == -1)
		// open return -1 if no process open file for reading
		*fd = open(fifo, O_WRONLY | O_NDELAY);
	if (*fd != -1) {
		// ignore the result
		write(*fd, msg, count);
		if (close_fifo) {  // unblock the reader thread
			close(*fd); *fd = -1;
		}
	}
	return true;
}

static void dispatch_common_payload(struct payload_header *header, const unsigned char *payload)
{
	if ((enum command_type)header->type_id != CMD_COMMON)
		return;

	switch ((enum common_command)header->cmd_id) {
		case COMMON_SYNC:
			break;
		case COMMON_TIME:
			break;
		case COMMON_STATUS: {
			struct common_status *p = (struct common_status *)payload;
			time_t t = p->time;
			struct tm *time = localtime(&t);
			int updays = p->uptime / 60 / 60 / 24;
			strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time);
			snprintf(message_buffer, sizeof(message_buffer),
				"SYSTEM [%" PRIu16 "] %s; Uptime: %d %s %02d:%02d:%02d; Vcc: %.02f; Lock: %s\n",
				header->seq_num, time_buffer, updays, (updays > 1 ? "days" : "day"),
				(p->uptime / 60 / 60) % 24, (p->uptime / 60) % 60, p->uptime % 60,
				p->vcc / 1000.0, p->nlock ? "OFF" : "ON");
			break;
		}
		case COMMON_RESET:
			break;
		case COMMON_PROGRAM:
			break;
		case COMMON_LOG_CLEAN:
			break;
		case COMMON_UNKNOWN:
		default:
			break;
	}

	message_buffer[sizeof(message_buffer) - 1] = '\0';
	write_fifo_and_close(FIFO_CMD, message_buffer, strlen(message_buffer), true);
}

static void dispatch_wdt_payload(struct payload_header *header, const unsigned char *payload)
{
	if ((enum command_type)header->type_id != CMD_WDT)
		return;

	switch ((enum wdt_command)header->cmd_id) {
		case WDT_KEEP_ALIVE:
			break;
		case WDT_DEACTIVATE:
			break;
		case WDT_STATUS: {
			struct wdt_status *p = (struct wdt_status *)payload;
			snprintf(message_buffer, sizeof(message_buffer),
				"WDT [%" PRIu16 "] Active: %s; Timeout: %" PRIu16 "s; MinDelta: %" PRIu16 "s; LogSize: %" PRIu16 "\n",
				header->seq_num, p->active ? "YES" : "NO", p->timeout, p->min_delta, p->log_length);
			message_buffer[sizeof(message_buffer) - 1] = '\0';
			write_fifo_and_close(FIFO_CMD, message_buffer, strlen(message_buffer), true);
			break;
		}
		case WDT_TIMEOUT:
			break;
		case WDT_LOG: {
			struct log_record *p;
			time_t t;
			struct tm *time;
			size_t event_list_len = sizeof(log_event_list) / sizeof(*log_event_list);

			for (int16_t i = 0; i < header->payload_size; i += sizeof(struct log_record)) {
				p = (struct log_record *)(payload + i);
				t = p->time;
				time = localtime(&t);
				strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time);
				snprintf(message_buffer, sizeof(message_buffer),
					"%s  %s\n", time_buffer, p->log_event < event_list_len ? log_event_list[p->log_event] : "UNEXPECTED");
				message_buffer[sizeof(message_buffer) - 1] = '\0';
				write_fifo(FIFO_CMD, message_buffer, strlen(message_buffer));
			}
			close(cmd_fifo_fd); cmd_fifo_fd = -1;
			break;
		}
		case WDT_UNKNOWN:
		default:
			break;
	}
}

static void dispatch_rng_payload(struct payload_header *header, const unsigned char *payload)
{
	if ((enum command_type)header->type_id != CMD_RNG)
		return;

	switch ((enum rng_command)header->cmd_id) {
		case RNG_FLOOD_ON:
			break;
		case RNG_FLOOD_OFF:
			break;
		case RNG_STATUS: {
			struct rng_status *p = (struct rng_status *)payload;
			snprintf(message_buffer, sizeof(message_buffer),
				"RNG [%" PRIu16 "] Threshold: %" PRIu8 "; Calibrated: %s; Flood: %s; Fault: %" PRIu16 "\n",
				header->seq_num, p->threshold, p->calibrated ? "YES" : "NO", p->flood ? "ON" : "OFF", p->fault);
			break;
		}
		case RNG_UNKNOWN:
		default:
			break;
	}

	message_buffer[sizeof(message_buffer) - 1] = '\0';
	write_fifo_and_close(FIFO_CMD, message_buffer, strlen(message_buffer), true);
}

static void dispatch_nrf_forward_payload(struct payload_header *header, const unsigned char *payload)
{
	if ((enum command_type)header->type_id != CMD_NRF_FORWARD)
		return;

	switch ((enum nrf_forward_command)header->cmd_id) {
		case NRF_FORWARD_L: {
			struct nrf_light *p = (struct nrf_light *)payload;
			snprintf(message_buffer, sizeof(message_buffer),
				"INSERT INTO sensor_light (id, uptime, light, vcc, tmp36, stat) VALUES "
				"('%" PRIu16 "', '%" PRIu32 "', '%" PRIu8 "', '%" PRId32 "', '%" PRId32 "', '%" PRIu8 "');\n",
				p->id, p->uptime, p->light, p->vcc, p->tmp36, p->stat);
			break;
		}
		case NRF_FORWARD_UNKNOWN:
		default:
			break;
	}

	message_buffer[sizeof(message_buffer) - 1] = '\0';
	write_fifo(FIFO_NRF, message_buffer, strlen(message_buffer));
}

void process_payload(struct payload_header *header, const unsigned char *payload)
{
	if (header == NULL || payload == NULL || header->payload_size <= 0)
		return;

	switch ((enum command_type)header->type_id) {
		case CMD_COMMON:
			dispatch_common_payload(header, payload);
			break;
		case CMD_WDT:
			dispatch_wdt_payload(header, payload);
			break;
		case CMD_RNG:
			dispatch_rng_payload(header, payload);
			break;
		case CMD_RNG_SEND:
			write_fifo(FIFO_RNG, (const char *)payload, header->payload_size);
			break;
		case CMD_NRF:
			break;
		case CMD_NRF_FORWARD:
			dispatch_nrf_forward_payload(header, payload);
			break;
		default:
			break;
	}
}

static void dispatch_common_confirmation(struct payload_header *header)
{
	if ((enum command_type)header->type_id != CMD_COMMON)
		return;

	switch ((enum common_command)header->cmd_id) {
		case COMMON_SYNC:
			break;
		case COMMON_TIME:
			break;
		case COMMON_STATUS:
			break;
		case COMMON_RESET:  // the confirmation usually come when the device is out of sync
			break;
		case COMMON_PROGRAM:
			// device must be programmed right after DTR is unlocked
			//snprintf(message_buffer, sizeof(message_buffer), "The device is ready to be programmed.\n");
			break;
		case COMMON_LOG_CLEAN:
			snprintf(message_buffer, sizeof(message_buffer), "The device log has successfully been cleaned out.\n");
			break;
		case COMMON_UNKNOWN:
		default:
			break;
	}

	message_buffer[sizeof(message_buffer) - 1] = '\0';
	write_fifo_and_close(FIFO_CMD, message_buffer, strlen(message_buffer), true);
}

static void dispatch_wdt_confirmation(struct payload_header *header)
{
	if ((enum command_type)header->type_id != CMD_WDT)
		return;

	switch ((enum wdt_command)header->cmd_id) {
		case WDT_KEEP_ALIVE:
			break;
		case WDT_DEACTIVATE:
			break;
		case WDT_STATUS:
			break;
		case WDT_TIMEOUT:
			break;
		case WDT_LOG:
			snprintf(message_buffer, sizeof(message_buffer), "The device log is empty.\n");
			break;
		case WDT_UNKNOWN:
		default:
			break;
	}

	message_buffer[sizeof(message_buffer) - 1] = '\0';
	write_fifo_and_close(FIFO_CMD, message_buffer, strlen(message_buffer), true);
}

void process_confirmation(struct payload_header *header)
{
	if (header == NULL || header->payload_size != 0)
		return;

	switch ((enum command_type)header->type_id) {
		case CMD_COMMON:
			dispatch_common_confirmation(header);
			break;
		case CMD_WDT:
			dispatch_wdt_confirmation(header);
			break;
		case CMD_RNG:
			break;
		case CMD_RNG_SEND:
			break;
		case CMD_NRF:
			break;
		case CMD_NRF_FORWARD:
			break;
		default:
			break;
	}
}

/*** WDT ***/

static void wdt_enable()
{
	pthread_spin_lock(&wrn_wdt_lock);
	if (time_delta(&wrn_wdt_keep_alive_sent) >= WDT_MIN_KEEP_ALIVE_INTERVAL) {
		// watchdog is making this call too often
		if (device_write_command("W0", "WDT:KEEP-ALIVE"))
			gettimeofday(&wrn_wdt_keep_alive_sent, NULL);
	}
	pthread_spin_unlock(&wrn_wdt_lock);
}

static void wdt_disable()
{
	pthread_spin_lock(&wrn_wdt_lock);
	device_write_command("W1", "WDT:DEACTIVATE");
	pthread_spin_unlock(&wrn_wdt_lock);
}

static void *wrn_wdt_read()
{
	char c;

	wdt_fifo_fd = open(arguments->wdt_fifo, O_RDONLY);
	while (true) {
		if (read(wdt_fifo_fd, &c, 1) == 1) {
			if (!wrn_wdt_nowayout) {
				wrn_wdt_ok_to_close = false;
				if (c == WDT_MAGIC_CHAR)
					wrn_wdt_ok_to_close = true;
			}

			wdt_enable();
		} else {
			wrn_wdt_release();
			wdt_fifo_fd = open(arguments->wdt_fifo, O_RDONLY);
		}
	} // infinite loop
}

bool wrn_wdt_open()
{
	int ret;

	wrn_wdt_ok_to_close = false;
	pthread_spin_init(&wrn_wdt_lock, PTHREAD_PROCESS_PRIVATE);

	ret = pthread_create(&wrn_wdt_thread, NULL, &wrn_wdt_read, NULL);
	if (ret != 0) {
		log_message(WRND_ERROR, "WDT: Failed to create the thread: %s", strerror(errno));
		return false;
	}

	return true;
}

void wrn_wdt_close()
{
	pthread_cancel(wrn_wdt_thread);
}

void wrn_wdt_release()
{
	if (wrn_wdt_ok_to_close)
		wdt_disable();
	else
		log_message(WRND_ERROR, "WDT: Device closed unexpectedly - timer will not stop");

	wrn_wdt_ok_to_close = false;
}
