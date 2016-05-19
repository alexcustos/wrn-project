// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#ifndef SERIALCOMMAND_H_
#define SERIALCOMMAND_H_

#include <avr/pgmspace.h>
#include <stdio.h>

#define MAX_SYNC_SEQUENCE 8
#define CMD_SIZE_SOFT_LIMIT 16

#include "HardwareSerial.h"

struct PayloadHeader {
	uint8_t type_id;
	uint8_t cmd_id;
	uint16_t seq_num;
	int16_t payload_size;
};

enum SerialCommandStatus {
	CS_TYPE,
	CS_ID,
	CS_ARG1,
	CS_ARG2,
	CS_COMPLETE
};

enum SerialCommandType {
	CMD_COMMON = 0,
	CMD_WDT,
	CMD_RNG,
	CMD_RNG_SEND,
	CMD_NRF,
	CMD_NRF_FORWARD,
	CMD_UNKNOWN
};

class SerialCommand
{
protected:
	static uint16_t seq_num;
	HardwareSerial *serial;
	SerialCommandStatus cmd_status;
	SerialCommandType cmd_type;
	int8_t cmd_id;
	int32_t cmd_arg1;
	int32_t cmd_arg2;
	PayloadHeader payload_header;

public:
	SerialCommand(HardwareSerial *);
	void set(SerialCommandType, int8_t, int32_t, int32_t);
	void reset();
	bool read();
	bool write(unsigned char);
	void print_cmd();
	bool is_complete() { return (cmd_status == CS_COMPLETE && cmd_type != CMD_UNKNOWN); }
	SerialCommandType get_type() { return cmd_type; }
	int8_t get_id() { return cmd_id; }
	int32_t get_arg1() { return cmd_arg1; }
	int32_t get_arg2() { return cmd_arg2; }
	bool send_sync();
	bool send_header(int16_t); // OK header: payload_size == 0; FAIL header: payload_size == -1
	bool send_payload(const unsigned char *, size_t);
};

inline bool SerialCommand::read()
{
	unsigned char c;
	uint8_t counter = 0;

	// comment out for the optimization
	//if (serial == NULL) return false;

	// allow external commands to be processed, NOTE: command must be reseted after run
	//if (is_complete()) return true;

	while (serial->available()) {
		c = serial->read();
		if (write(c)) return true;
#ifdef DEBUG
		else if (c == '\n') printf_P(PSTR("NOOP\r\n"));
#endif
		if (counter++ >= CMD_SIZE_SOFT_LIMIT) break;
	}

	return false;
}


#endif /* SERIALCOMMAND_H_ */
