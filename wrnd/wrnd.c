// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <libgen.h>
#include <sys/time.h>
#include "wrnd.h"
#include "serialport.h"
#include "utils.h"
#include "log.h"
#include "devices.h"

static bool server_running = true;
static int exit_code = EXIT_FAILURE;
int serial_fd = -1;

struct arguments default_arguments = {
	.device_port = "/dev/ttyS0",
	.baud_rate = 57600,
	.vtime = 5,
	.rng_fifo = "/run/wrnd/rng.fifo",
	.nrf_fifo = "/run/wrnd/nrf.fifo",
	.pid_file = "/run/wrnd/pid",
	.wdt_fifo = "/run/wrnd/wdt.fifo",
	.wdt_timeout = 180,
	.wdt_nowayout = false,
	.verbose = 0,
	.daemonize = false
};
struct arguments *arguments = &default_arguments;

static void usage(char *progname)
{
	fprintf(stderr, "%s version %d.%d, usage:\n", progname, MAJOR_VERSION, MINOR_VERSION);
	fprintf(stderr, "%s [options]\n", progname);
	fprintf(stderr, "Options (default value in parenthesis):\n");
	fprintf(stderr, "  -h, --help                  Print this help message\n");
	fprintf(stderr, "  -D, --device-port=port      Serial port of the device (%s)\n", default_arguments.device_port);
	fprintf(stderr, "  -b, --baud-rate=rate        Baud rate in bps (%u)\n", default_arguments.baud_rate);
	fprintf(stderr, "  -t, --timeout=vtime         Tenths of a second for serial port read cycle (%u)\n", default_arguments.vtime);
	fprintf(stderr, "  -r, --rng-fifo=file         FIFO for RNG (%s)\n", default_arguments.rng_fifo);
	fprintf(stderr, "  -n, --nrf-fifo=file         FIFO for nRF24l01+ (%s)\n", default_arguments.nrf_fifo);
	fprintf(stderr, "  -p, --pid-file=file         Name for the PID file (%s)\n", default_arguments.pid_file);
	fprintf(stderr, "  -w, --wdt-fifo=file         FIFO for the watchdog daemon (%s)\n", default_arguments.wdt_fifo);
	fprintf(stderr, "  -T, --wdt-timeout=timeout   The watchdog trigger timeout [min: %u, max: %u] (%u)\n",
		WDT_TIMEOUT_MIN, WDT_TIMEOUT_MAX, default_arguments.wdt_timeout);
	fprintf(stderr, "  -N, --wdt-nowayout          Watchdog cannot be stopped once started\n");
	fprintf(stderr, "  -v, --verbose=level         Verbose messages level [0|1|2|3] (%u)\n", default_arguments.verbose);
	fprintf(stderr, "  -d, --daemonize             Run in the background as a daemon\n");
	exit(EXIT_FAILURE);
}

static void term_signal(int signo)
{
	exit_code = EXIT_SUCCESS;
    server_running = false;
}

static void logrotate_signal(int signo)
{
	reopen_logs();
}

static void do_loop()
{
	int n = 0, bi = 0;
	unsigned char c;
	unsigned char buffer[SERIAL_RX_BUFFER_SIZE];
	struct payload_header header;
	enum transmission_status status = TX_UNKNOWN;
	uint16_t seq_num = 0;
	bool sequence_found = false;
	int sync_retried = 0;
	struct timeval sync_started = {.tv_sec = 0, .tv_usec = 0};

	if (sizeof(header) > SERIAL_RX_BUFFER_SIZE) {
		log_message(WRND_ERROR, "Serial RX buffer is too small");
		server_running = false;
	}

	while (server_running) {
		if (status == TX_UNKNOWN) {
			if (serial_fd != -1)
				close(serial_fd);
			serial_fd = serialport_init(arguments->device_port, arguments->baud_rate, arguments->vtime);
			if (serial_fd < 0)
				break;

			if ((enum verbose_level)arguments->verbose > VERBOSE_L0)
				log_message(WRND_COMMON, "Sync with the device is about to be started");

			if (!device_write_command("R1", "RNG:FLOOD-OFF"))
				break;

			while (read(serial_fd, &c, 1) > 0);  // clean input buffer
			if (!device_send_sync(SERIAL_RX_SYNC_SEQUENCE))
				break;

			gettimeofday(&sync_started, NULL);
			bi = 0;
			status = TX_SYNC;
		} else if (status == TX_SYNC && time_delta(&sync_started) > SERIAL_SYNC_TIMEOUT) {
			sync_retried++;
			if (sync_retried >= SERIAL_SYNC_RETRY) {
				log_message(WRND_ERROR, "Sync with the device failed");
				break;
			}
			bi = 0;
			status = TX_UNKNOWN;
			continue;
		}

		n = read(serial_fd, &c, 1);
		if (n == -1) {
			log_message(WRND_ERROR, "Could not read the serial port: %s", strerror(errno));
			break;
		} else if (n == 0)
			continue;

		*(buffer + bi) = c;
		bi++;
		if (status == TX_HEADER && bi >= sizeof(header)) {
			bi = 0;
			memcpy(&header, &buffer, sizeof(header));

			if ((enum verbose_level)arguments->verbose > VERBOSE_L1)
				log_device_header(&header);

			if (header.seq_num != seq_num) {
				log_message(WRND_ERROR, "The daemon is out of sync with the device %d:[%d]", header.seq_num, seq_num);
				status = TX_UNKNOWN;
				seq_num = 0;
				continue;
			} else
				seq_num++;

			if (header.payload_size < 0)
				log_device_error(&header);
			else if (header.payload_size > 0)
				status = TX_PAYLOAD;
			else if ((enum command_type)header.type_id == CMD_COMMON && (enum common_command)header.cmd_id == COMMON_RESET) {
				log_message(WRND_COMMON, "The daemon has been received the device RESET command without loss of sync");
				// it is a very rare behaviour, so it is not a problem to resync the daemon for the device initialization
				status = TX_UNKNOWN;
				seq_num = 0;
				continue;
			} else
				process_confirmation(&header);

		} else if (status == TX_SYNC && c == 0xFF && bi >= SERIAL_RX_SYNC_SEQUENCE) {
			sequence_found = true;
			for (int i = 2; i <= SERIAL_RX_SYNC_SEQUENCE; i++) {
				if (buffer[bi - i] != 0xFF) {
					sequence_found = false; 
					break;
				}
			}
			if (sequence_found) {
				log_message(WRND_COMMON, "The daemon has been successfully synced");
				if (!init_device())
					break;
				sync_retried = 0;
				bi = 0;
				status = TX_HEADER;
				continue;
			}

		} else if (status == TX_PAYLOAD && bi >= header.payload_size) {
			if ((enum verbose_level)arguments->verbose > VERBOSE_L2)
				log_device_payload(&header, buffer);

			process_payload(&header, buffer);
			bi = 0;
			status = TX_HEADER;
		}

		if (bi >= SERIAL_RX_BUFFER_SIZE) {
			log_message(WRND_ERROR, "Serial RX buffer overflow detected %d:[%d]", bi, SERIAL_RX_BUFFER_SIZE);
			if (status == TX_SYNC) {
				bi = 0;
				status = TX_UNKNOWN;
				continue;
			}
			else
				break;
		}
	} // while server_running

	close_device();
}

int main(int argc, char *const argv[])
{
	int opt = 0;
	char *progname = basename(argv[0]);
	char *opts = "hD:b:t:r:n:p:w:T:Nv:d";
	struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"device-port", required_argument, NULL, 'D'},
		{"baud-rate", required_argument, NULL, 'b'},
		{"timeout", required_argument, NULL, 't'},
		{"rng-fifo", required_argument, NULL, 'r'},
		{"nrf-fifo", required_argument, NULL, 'n'},
		{"pid-file", required_argument, NULL, 'p'},
		{"wdt-fifo", required_argument, NULL, 'w'},
		{"wdt-timeout", required_argument, NULL, 'T'},
		{"wdt-nowayout", no_argument, NULL, 'N'},
		{"verbose", required_argument, NULL, 'v'},
		{"daemonize", no_argument, NULL, 'd'},
		{NULL, 0, NULL, 0}
	};

	while ((opt = getopt_long(argc, argv, opts, long_options, NULL)) != EOF) {
		switch (opt) {
		case 'h':
			usage(progname);
			break;
		case 'D':
			if (optarg != NULL && strlen(optarg) > 0)
				arguments->device_port = optarg;
			break;
		case 'b':
			if (optarg != NULL && strlen(optarg) > 0)
				arguments->baud_rate = (unsigned int)strtoul(optarg, NULL, 10);
			break;
		case 't':
			if (optarg != NULL && strlen(optarg) > 0)
				arguments->vtime = (unsigned char)strtoul(optarg, NULL, 10);
			break;
		case 'r':
			if (optarg != NULL && strlen(optarg) > 0)
				arguments->rng_fifo = optarg;
			break;
		case 'n':
			if (optarg != NULL && strlen(optarg) > 0)
				arguments->nrf_fifo = optarg;
			break;
		case 'p':
			if (optarg != NULL && strlen(optarg) > 0)
				arguments->pid_file = optarg;
			break;
		case 'w':
			if (optarg != NULL && strlen(optarg) > 0)
				arguments->wdt_fifo = optarg;
			break;
		case 'T':
			if (optarg != NULL && strlen(optarg) > 0)
				arguments->wdt_timeout = (unsigned int)strtoul(optarg, NULL, 10);
			if (arguments->wdt_timeout < WDT_TIMEOUT_MIN)
				arguments->wdt_timeout = WDT_TIMEOUT_MIN;
			if (arguments->wdt_timeout > WDT_TIMEOUT_MAX)
				arguments->wdt_timeout = WDT_TIMEOUT_MAX;
			break;
		case 'N':
			arguments->wdt_nowayout = true;
			break;
		case 'v':
			if (optarg != NULL && strlen(optarg) > 0) {
				arguments->verbose = (unsigned char)strtoul(optarg, NULL, 10);
				if (arguments->verbose > MAX_VERBOSE_LEVEL)
					arguments->verbose = MAX_VERBOSE_LEVEL;
			}
			break;
		case 'd':
			arguments->daemonize = true;
			break;
		default:
			usage(progname);
		}
	}

	if(geteuid() != 0) {
		fprintf(stderr, "The daemon must be started with root privileges.\n");
		return EXIT_FAILURE;
	}

	if (!open_logs())
		return EXIT_FAILURE;

	if (!check_pid_file(arguments->pid_file))
		return EXIT_FAILURE;

	if (mkdir(DEFAULT_PIDDIR, 0755) && errno != EEXIST) {
		log_message(WRND_ERROR, "Cannot create directory %s: %s", DEFAULT_PIDDIR, strerror(errno));
		return EXIT_FAILURE;
	}

	if (!init_fifos())
		return EXIT_FAILURE;

	// check if the serial port available
	serial_fd = serialport_init(arguments->device_port, arguments->baud_rate, arguments->vtime);
	if (serial_fd < 0)
		return EXIT_FAILURE;

	if (arguments->daemonize && daemon(0, 0) < 0) {
		log_message(WRND_ERROR, "Failed to daemonize: %s", strerror(errno));
		close(serial_fd);
		return EXIT_FAILURE;
	}

	if (!write_pid_file(arguments->pid_file)) {
		close(serial_fd);
		return EXIT_FAILURE;
	}

	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, logrotate_signal);
	signal(SIGINT, term_signal);
	signal(SIGTERM, term_signal);

	if (!wrn_wdt_open()) {
		close(serial_fd);
		return EXIT_FAILURE;
	}

	log_message(WRND_COMMON, "+++ Daemon %s has been started", progname);
	do_loop();

    unlink(arguments->pid_file);
    log_message(WRND_COMMON, "Daemon %s has been stopped", progname);

	wrn_wdt_close();

	if (serial_fd != -1)
		close(serial_fd);
	close_logs();
	return exit_code;
}
