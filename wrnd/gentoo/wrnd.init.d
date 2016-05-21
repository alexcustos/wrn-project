#!/sbin/openrc-run
# Copyright (c) 2016 Aleksandr Borisenko
# Distributed under the terms of the GNU General Public License v2

extra_started_commands="reload"

wrnd_daemon="/usr/local/bin/wrnd"
if [ -n "${WRND_PIDFILE}" ]; then
	PIDFILE="${WRND_PIDFILE}"
else
	PIDFILE="/run/wrnd/pid"
fi

depend() {
	# to provide RNG fifo
	before rngd
	# to provide WDT fifo
	before watchdog
}

start() {
	local OPTIONS
	OPTIONS="--daemonize"

	if [ -n "${WRND_DEVICE}" ]; then
		OPTIONS="${OPTIONS} --device-port=${WRND_DEVICE}"
	fi
	if [ -n "${WRND_BAUDRATE}" ]; then
		OPTIONS="${OPTIONS} --baud-rate=${WRND_BAUDRATE}"
	fi
	if [ -n "${WRND_TIMEOUT}" ]; then
		OPTIONS="${OPTIONS} --timeout=${WRND_TIMEOUT}"
	fi
	if [ -n "${WRND_RNGFIFO}" ]; then
		OPTIONS="${OPTIONS} --rng-fifo=${WRND_RNGFIFO}"
	fi
	if [ -n "${WRND_NRFFIFO}" ]; then
		OPTIONS="${OPTIONS} --nrf-fifo=${WRND_NRFFIFO}"
	fi
	if [ -n "${WRND_WDTFIFO}" ]; then
		OPTIONS="${OPTIONS} --wdt-fifo=${WRND_WDTFIFO}"
	fi
	if [ -n "${WRND_WDTTIMEOUT}" ]; then
		OPTIONS="${OPTIONS} --wdt-timeout=${WRND_WDTTIMEOUT}"
	fi
	if [ -n "${WRND_OPTS}" ]; then
		OPTIONS="${OPTIONS} ${WRND_OPTS}"
	fi

	ebegin "Starting WRN Daemon"
	start-stop-daemon --start --pidfile "${PIDFILE}" --exec ${wrnd_daemon} -- \
		${OPTIONS} --pid-file=${PIDFILE}
	eend $?
}

stop() {
	ebegin "Stopping WRN Daemon"
	start-stop-daemon --stop --pidfile "${PIDFILE}" --exec ${wrnd_daemon}
	eend $?
}

reload() {
    ebegin "Reopening WRN Daemon log files"
    start-stop-daemon --signal HUP --pidfile "${PIDFILE}"
    eend $?
}
