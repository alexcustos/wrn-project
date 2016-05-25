// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#ifndef RNGDEVICE_H_
#define RNGDEVICE_H_

#include "main.h"

#define RNG_PAYLOAD_SIZE 64
#define RNG_FAST_CALIBRATION 2048

enum RNGCommand {
	RNG_FLOOD_ON = 0,
	RNG_FLOOD_OFF,
	RNG_STATUS,
	RNG_UNKNOWN
};

enum RNGSendCommand {
	RNG_SEND_PAYLOAD = 0,
	RNG_SEND_UNKNOWN
};

struct RNGStatusPayload
{
	uint8_t threshold;
	uint8_t calibrated;
	uint8_t flood;
	uint16_t fault;
};

class RNGDevice : public Device
{
protected:
	bool flood;
	uint8_t byte;
	uint8_t threshold;
	uint16_t num_measures;
	uint16_t measure_limit;
	uint16_t pan_left;
	uint16_t pan_right;
	uint8_t payload_len;
	uint16_t fault;
	bool bit_flip;
	uint8_t payload[RNG_PAYLOAD_SIZE];

	bool send_status(SerialCommand *);
	uint8_t read_measure();
public:
	RNGDevice();
	bool run(SerialCommand *);
	bool read(SerialCommand *);
};

inline uint8_t RNGDevice::read_measure()
{
	uint8_t low, high;

	// REFS: 01 - reference AVCC with external capacitor at AREF pin
	// MUX: 0101 - input channel ADC5
	ADMUX = _BV(REFS0) | _BV(MUX2) | _BV(MUX0);

	ADCSRA |= _BV(ADSC);  // ADC Start Conversion
	while (bit_is_set(ADCSRA, ADSC));  // when the conversion is complete, it returns to zero

	// ADC Data Register; ADCL must be read first, then ADCH
	low = ADCL;
	high = ADCH;

	// lower precision: 10 bit to 8 bit
	return (high << 6) | (low >> 2);
}

// This functions is little bit confusing, but I believe it is the best possible solution.
// The other options demands large buffer or strong calculations to balance the threshold.
inline bool RNGDevice::read(SerialCommand *cmd)
{
	uint8_t measure;
	bool balance;
	uint16_t acceptable_fault;

	// comment out for the optimization
	//if (cmd == NULL) return false;

	measure = read_measure();
	// count starts from 1 and goes up to overflow (natural or artificial)
	num_measures++;

	if (num_measures == measure_limit) {
		balance = false;
		if (pan_left > pan_right) fault = pan_left - pan_right;
		else fault = pan_right - pan_left;
		acceptable_fault = ((measure_limit - 1) / 256 + 1) * 3;

		if (fault > acceptable_fault) {
			if (pan_right > pan_left && threshold < uint8_t(-1)) threshold++;
			else if (pan_left >= pan_right && threshold > 0) threshold--;
		} else balance = true;

		IF_DEBUG(printf_P(PSTR("[Calibration] Measures:%u; Threshold:%u; Limit: %u; Max: %u; [%u:%u=%u]; Balance:%s\r\n"),
		num_measures - 1, threshold, measure_limit - 1, acceptable_fault, pan_left, pan_right, fault, balance ? "YES" : "NO"));

		if (measure_limit) num_measures = 0;
		pan_left = 0;
		pan_right = 0;

		// switch to calibration mode in case the problem is found
		if (threshold == 0 || fault == uint16_t(-1)) {
			measure_limit = RNG_FAST_CALIBRATION;
			balance = false;
		}

		if (balance && measure_limit) {  // balance just found
			measure_limit = 0;
			return false;  // pass this cycle to start new byte from the first bit
		}
	} else {  // drop one measure per calibration cycle to prevent possible overflow
		if (measure <= threshold) pan_left++;
		else pan_right++;
	}

	if (measure_limit) return false;

	byte <<= 1;
	if (measure > threshold) byte |= 0b00000001;
	// too many monobit failures, need bias removal
	byte ^= bit_flip;
	bit_flip = !bit_flip;

	// num_measures is power of 2, so overflow is not a problem
	if (flood && num_measures % 8 == 0) {
		if (payload_len < sizeof payload) {
			payload[payload_len++] = byte;
		} else {
			// payload_len will be reseted in the run()
			cmd->set(CMD_RNG_SEND, RNG_SEND_PAYLOAD, 0, 0);
			return true;
		}
	}

	return false;
}

#endif /* RNGDEVICE_H_ */
