// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#include <ctype.h>
#include <inttypes.h>
#include <string.h>
#include <avr/pgmspace.h>
#include <stdio.h>
#include "SerialCommand.h"
#include "main.h"

uint16_t SerialCommand::seq_num = 0;

SerialCommand::SerialCommand(HardwareSerial *serial) : serial(serial),
	cmd_status(CS_TYPE), cmd_type(CMD_UNKNOWN), cmd_id(0), cmd_arg1(0), cmd_arg2(0)
{
}

void SerialCommand::reset()
{
	cmd_status = CS_TYPE;
	cmd_type = CMD_UNKNOWN;
	cmd_id = 0;
	cmd_arg1 = 0;
	cmd_arg2 = 0;
}

void SerialCommand::set(SerialCommandType type, int8_t id, int32_t arg1, int32_t arg2)
{
	cmd_status = CS_COMPLETE;
	cmd_type = type;
	cmd_id = id;
	cmd_arg1 = arg1;
	cmd_arg2 = arg2;
}

// return true when the command is completed
// W == W0:0:0
bool SerialCommand::write(unsigned char c)
{
	if (cmd_status == CS_COMPLETE) reset();  // should be processed already

	if (c == '\r') return false;
	if (c == ':') {
		if (cmd_status == CS_ID) cmd_status = CS_ARG1;
		else if (cmd_status == CS_ARG1 ) cmd_status = CS_ARG2;
		else reset();

		return false;
	}
	if (c == '\n') {
		if (cmd_type != CMD_UNKNOWN) {
			cmd_status = CS_COMPLETE;
			return true;
		} else reset();

		return false;
	}
	switch (cmd_status) {
	case CS_TYPE:
		if (c == 'C' || c == 'c') cmd_type = CMD_COMMON;
		else if (c == 'W' || c == 'w') cmd_type = CMD_WDT;
		else if (c == 'R' || c == 'r') cmd_type = CMD_RNG;
		else if (c == 'N' || c == 'n') cmd_type = CMD_NRF;
		else cmd_type = CMD_UNKNOWN;
		// Scan for the first valid command
		if (cmd_type != CMD_UNKNOWN) cmd_status = CS_ID;
		break;
	case CS_ID:
		if (isdigit(c) && cmd_id < 10) cmd_id = cmd_id * 10 + (c - '0');
		else reset();
		break;
	case CS_ARG1:
		// Overflow here is perfectly normal
		if (isdigit(c)) cmd_arg1 = cmd_arg1 * 10 + (c - '0');
		else reset();
		break;
	case CS_ARG2:
		// Overflow here is perfectly normal
		if (isdigit(c)) cmd_arg2 = cmd_arg2 * 10 + (c - '0');
		else reset();
		break;
	default:
		break;
	}

	return false;
}

void SerialCommand::print_cmd()
{
	IF_DEBUG(printf_P(PSTR("%d[%d]:%ld:%ld\r\n"), (int)cmd_type, cmd_id, cmd_arg1, cmd_arg2));
}

bool SerialCommand::send_header(int16_t payload_size)
{
	if (serial == NULL) return false;

	payload_header.type_id = (uint8_t)cmd_type;
	payload_header.cmd_id = (uint8_t)cmd_id;
	payload_header.seq_num = seq_num;
	payload_header.payload_size = payload_size;
	seq_num++;

#ifdef DEBUG
	printf_P(PSTR("Header #%d [%d:%d]:%d\r\n"),
		payload_header.seq_num, payload_header.type_id, payload_header.cmd_id, payload_header.payload_size);
	return true;
#else
	size_t n = serial->write((const unsigned char *)&payload_header, sizeof payload_header);
	if (n == (sizeof payload_header)) return true;
#endif

	return false;
}

bool SerialCommand::send_payload(const unsigned char *payload, size_t size)
{
	if (serial == NULL || payload == NULL) return false;

#ifdef DEBUG
	return true;
#else
	size_t n = serial->write(payload, size);
	if (n == size) return true;
#endif

	return false;
}

bool SerialCommand::send_sync()
{
	if (serial == NULL || cmd_arg1 <= 0 || cmd_arg1 > MAX_SYNC_SEQUENCE) return false;

	unsigned char sync_buffer[cmd_arg1];
	memset(&sync_buffer, 0xFF, cmd_arg1);
	seq_num = 0;

#ifdef DEBUG
	printf_P(PSTR("Sync sequence: %ld sent\r\n"), cmd_arg1);
	return true;
#else
	size_t n = serial->write((const unsigned char *)&sync_buffer, cmd_arg1);
	if (n == cmd_arg1) return true;
#endif

	return false;
}
