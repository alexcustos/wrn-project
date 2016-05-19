// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#include <avr/wdt.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include "Time.h"
#include "Devices.h"
#include "Utils.h"
#include "EepromLog.h"
#include "main.h"

// *** COMMON

CommonDevice::CommonDevice() : boot_logged(false)
{
}

bool CommonDevice::confirm_boot(SerialCommand *cmd)
{
	if ( cmd == NULL || cmd->get_type() != CMD_COMMON || cmd->get_id() != COMMON_RESET) return false;

	if (!send(cmd)) return false;

	return true;
}

bool CommonDevice::run(SerialCommand *cmd)
{
	if ( cmd == NULL || cmd->get_type() != CMD_COMMON) return false;

	switch ((CommonCommand)cmd->get_id()) {
	case COMMON_SYNC:  // C0:3
		if (!cmd->send_sync()) return false;
		break;
	case COMMON_TIME:  // C1:1460792071
		if (!time(cmd->get_arg1())) return false;
		break;
	case COMMON_STATUS:  // C2
		if (!send_status(cmd)) return false;
		break;
	case COMMON_RESET:  // C3
		wdt_enable(WDTO_30MS);
		while(1) {};
		break;
	case COMMON_PROGRAM:  // C4
		// NB!!! device must be programmed right after DTR is unlocked
		DDRC &= ~_BV(DDC2);  // NLOCK input
		break;
	case COMMON_LOG_CLEAN:  // C5
		sys_log.clean();
		if (!send(cmd)) return false;  // log was successfully cleaned
		break;
	default:
		return false;
	}
	return true;
}

bool CommonDevice::time(int32_t t)
{
	if (t > 0) {
		sys_time.set_time(t);
		if (!boot_logged) {
			sys_log.write(sys_time.now(), LOG_BOOT);
			boot_logged = true;
		}
		return true;
	}
	return false;
}

bool CommonDevice::send_status(SerialCommand *cmd)
{
	CommonStatusPayload status;
	status.time = sys_time.now();
	status.uptime = sys_time.get_uptime();
	status.vcc = read_vcc();
	status.nlock = !!(DDRC & _BV(DDC2) ? PORTC & _BV(PORTC2) : PINC & _BV(PINC2));

	IF_DEBUG(printf_P(PSTR("Payload [Common:Status] Time:%ld; Uptime:%lu; Vcc:%ld; nLock:%u\r\n"),
		status.time, status.uptime, status.vcc, status.nlock));

	return send(cmd, (const unsigned char *)&status, sizeof status);
}

// *** NRF

NRFDevice::NRFDevice() : network(NULL)
{
}

bool NRFDevice::run(SerialCommand *cmd)
{
	// comment out for the optimization
	//if (network == NULL || cmd == NULL) return false;

	SerialCommandType type = cmd->get_type();

	if (type == CMD_NRF_FORWARD) {  // cmd from read()
		switch ((NRFForwardCommand)cmd->get_id()) {
			case NRF_FORWARD_L: {
				struct NRFLPayload payload;

				network->read(header, &payload, sizeof(payload));
				IF_DEBUG(printf_P(PSTR("Payload [L] #%u; Uptime: %lu; Light: %u; Vcc: %ld; TMP36: %ld.%ld; STATUS: %u\r\n"),
					payload.id, payload.uptime, payload.light, payload.vcc, payload.tmp36/1000, payload.tmp36%1000, payload.stat));

				if (!send(cmd, (const unsigned char *)&payload, sizeof payload)) return false;
				break;
			}
			default:
				return false;
		}
	} else if (type == CMD_NRF) return false;

	return true;
}
