// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>
#include "WDTDevice.h"
#include "EepromLog.h"
#include "Time.h"
#include "main.h"

#define WDT_TIMEOUT_DEFAULT 180  // sec
#define WDT_TIMEOUT_MIN 30  // sec
#define WDT_TIMEOUT_MAX 300  // sec


WDTDevice::WDTDevice() : active(false), timeout(WDT_TIMEOUT_DEFAULT), min_delta(WDT_TIMEOUT_DEFAULT), keep_alive_uptime(0)
{
	DDRC |= _BV(DDC1);  // RESET:OUTPUT
	PORTC &= ~_BV(PORTC1);  // RESET:LOW
}

bool WDTDevice::run(SerialCommand *cmd)
{
	if ( cmd == NULL || cmd->get_type() != CMD_WDT) return false;

	switch ((WDTCommand)cmd->get_id()) {
	case WDT_KEEP_ALIVE:  // W0
		if (!active) min_delta = timeout;
		active = true;
		keep_alive_uptime = sys_time.get_uptime();
		break;
	case WDT_DEACTIVATE:  // W1
		active = false;	
		break;
	case WDT_STATUS:  // W2
		if (!send_status(cmd)) return false;
		break;
	case WDT_TIMEOUT:  // W3:180
		if (!set_timeout(cmd->get_arg1())) return false;
		break;
	case WDT_LOG:  // W4:10
		if (!send_log(cmd)) return false;
		break;
	default:
		return false;
	}

	return true;
}

bool WDTDevice::set_timeout(int32_t t)
{
	if (t < WDT_TIMEOUT_MIN || t > WDT_TIMEOUT_MAX) return false;

	if (timeout != t) {
		timeout = t;
		min_delta = t;
	}
	return true;
}

bool WDTDevice::send_status(SerialCommand *cmd)
{
	WDTStatusPayload status;
	status.active = active;
	status.timeout = timeout;
	status.min_delta = min_delta;
	status.log_length = sys_log.length();

	IF_DEBUG(printf_P(PSTR("Payload [WDT:Status] Active:%u; Timeout:%u; MinDelta:%u; LogLength:%u\r\n"),
		status.active, status.timeout, status.min_delta, status.log_length));

	return send(cmd, (const unsigned char *)&status, sizeof status);
}

bool WDTDevice::send_log(SerialCommand *cmd)
{
	uint16_t size, num, n;
	bool ret = true;  // empty log it's perfectly normal
	LogRecord *record;

	num = sys_log.length();
	n = (uint16_t)cmd->get_arg1();
	if (n > 0 && num > n) num = n;
	size = num * sizeof(LogRecord);

	if (!cmd->send_header(size)) return false;

	sys_log.set_reverse(false);  // reset cursor
	sys_log.set_limit(num);
	for (uint16_t i = 0; i < num; i++) {
		record = sys_log.read();
		IF_DEBUG(printf_P(PSTR("Payload [WDT:LogRecord] Time:%ld; Event:%u\r\n"),
			record->time, record->log_event));
		ret = cmd->send_payload((const unsigned char *)record, sizeof(LogRecord));
		if (!ret) break;
	}

	return ret;
}
