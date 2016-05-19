// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#include <avr/io.h>
#include <util/delay.h>
#include <stdio.h>
#include <stdlib.h>
#include "Time.h"
#include "Devices.h"
#include "RNGDevice.h"
#include "main.h"


RNGDevice::RNGDevice() : flood(false), byte(0), threshold(127), num_measures(0),
	measure_limit(RNG_FAST_CALIBRATION), pan_left(0), pan_right(0), payload_len(0), fault(0), bit_flip(false)
{
	memset(&payload, 0, sizeof payload);
}

bool RNGDevice::run(SerialCommand *cmd)
{
	// comment out for the optimization
	//if (cmd == NULL) return false;

	SerialCommandType type = cmd->get_type();

	if (type == CMD_RNG_SEND) {  // cmd from read()
		switch ((RNGSendCommand)cmd->get_id()) {
			case RNG_SEND_PAYLOAD: {
				// in case it's forced
				if (payload_len <= 0) return false;
#ifdef DEBUG
				printf_P(PSTR("Payload [RNG]"));
				for (uint16_t i = 0; i < payload_len; i++) printf_P(PSTR(" %02X"), payload[i]);
				printf_P(PSTR("\r\n"));
#endif
				if (!send(cmd, (const unsigned char *)&payload, payload_len)) {
					payload_len = 0;
					return false;
				}
				payload_len = 0;
				break;
			}
			default:
				return false;
		}
	} else if (type == CMD_RNG) {
		switch ((RNGCommand)cmd->get_id()) {
		case RNG_FLOOD_ON:  // R0
			flood = true;
			break;
		case RNG_FLOOD_OFF:  // R1
			payload_len = 0;
			flood = false;
			break;  // calibration will be started on-the-fly
		case RNG_STATUS:  // R2
			if (!send_status(cmd)) return false;
			break;
		default:
			return false;
		}
	}

	return true;
}

bool RNGDevice::send_status(SerialCommand *cmd)
{
	RNGStatusPayload status;

	status.threshold = threshold;
	status.calibrated = !measure_limit;
	status.flood = flood;
	status.fault = fault;

	IF_DEBUG(printf_P(PSTR("Payload [RNG:Status] Threshold:%u; Calibrated:%u; Flood:%u; Fault:%u\r\n"),
		status.threshold, status.calibrated, status.flood, status.fault));

	return send(cmd, (const unsigned char *)&status, sizeof status);
}
