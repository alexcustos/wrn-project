// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#ifndef DEVICES_H_
#define DEVICES_H_

#include <inttypes.h>
#include <RF24Network.h>
#include "HardwareSerial.h"
#include "SerialCommand.h"

class Device
{
protected:
	bool send(SerialCommand *cmd, const unsigned char *payload = NULL, size_t size = 0);
public:
	Device() {};
	virtual bool run(SerialCommand *); // return payload size; -1 error code; send_header if payload_size > 0
};

inline bool Device::send(SerialCommand *cmd, const unsigned char *payload /* NULL */, size_t size /* 0 */)
{
	if (cmd == NULL) return false;
	if (!cmd->send_header(size)) return false;

	if (payload == NULL && size == 0) return true;
	return cmd->send_payload(payload, size);
}


// *** COMMON

enum CommonCommand {
	COMMON_SYNC = 0,
	COMMON_TIME,
	COMMON_STATUS,
	COMMON_RESET,
	COMMON_PROGRAM,
	COMMON_LOG_CLEAN,
	COMMON_UNKNOWN
};

struct CommonStatusPayload
{
	int32_t time;
	uint32_t uptime; // sec
	int32_t vcc;
	uint8_t nlock;
};

class CommonDevice : public Device
{
protected:
	bool boot_logged;

	bool time(int32_t);
	bool send_status(SerialCommand *);
public:
	CommonDevice();
	bool confirm_boot(SerialCommand *);
	bool run(SerialCommand *);
};

// *** NRF

enum NRFCommand {
	NRF_UNKNOWN = 0
};

enum NRFForwardCommand {
	NRF_FORWARD_L = 0,  // NRFLPayload
	NRF_FORWARD_UNKNOWN
};

struct NRFLPayload  // Light sensor: just forward
{
	uint16_t id;
	uint32_t uptime;
	uint8_t light;
	int32_t vcc;
	int32_t tmp36;
	uint8_t stat;
};

class NRFDevice : public Device
{
private:
	RF24Network *network;
	RF24NetworkHeader header;
public:
	NRFDevice();
	bool run(SerialCommand *);
	void setup(RF24Network *net) { network = net; };
	bool read(SerialCommand *);
};

inline bool NRFDevice::read(SerialCommand *cmd)
{
	// the device should work without the radio
	if (network == NULL) return false; // || cmd == NULL

	network->update();

	if (network->available()) {
		network->peek(header);
		if (header.type == 'L') {
			cmd->set(CMD_NRF_FORWARD, NRF_FORWARD_L, 0, 0);
			return true;
		}
	}

	return false;
}

#endif /* DEVICES_H_ */
