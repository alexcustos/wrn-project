#!/bin/bash
# Copyright (c) 2016 Aleksandr Borisenko
# Distributed under the terms of the GNU General Public License v2

WRND_CONFIG="/etc/conf.d/wrnd"
SENSORS_DB="./wrnsensors.db"
LOG="/var/log/wrnd/sensors.log"
SCRIPT_DIR=`dirname $0`

# DEFAULT
WRND_NRFFIFO="/run/wrnd/nrf.fifo"

if [ -f ${WRND_CONFIG} ]; then
    . ${WRND_CONFIG}
fi

if [ ! -p "${WRND_NRFFIFO}" ]; then
    echo_to_stderr "The FIFO ${WRND_NRFFIFO} cannot be opened for reading."
    exit 1
fi

if ! sqlite3 ${SENSORS_DB} <<< .schema | grep -q 'CREATE TABLE sensor_light '; then
	sqlite3 ${SENSORS_DB} <${SCRIPT_DIR}/sensor_light.sql
fi

while true
do
    if read sql_line <${WRND_NRFFIFO}; then
        sqlite3 ${SENSORS_DB} "PRAGMA foreign_keys = ON; ${sql_line}" >>${LOG} 2>&1
    fi
done
