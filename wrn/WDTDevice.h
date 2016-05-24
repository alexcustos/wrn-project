// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#ifndef WDTDEVICE_H_
#define WDTDEVICE_H_

#include <avr/io.h>
#include <stdio.h>
#include "Devices.h"
#include "EepromLog.h"
#include "Time.h"


enum WDTCommand {
	WDT_KEEP_ALIVE = 0,
	WDT_DEACTIVATE,
	WDT_STATUS,
	WDT_TIMEOUT,
	WDT_LOG,
	WDT_UNKNOWN
};

struct WDTStatusPayload
{
	uint8_t active;
	uint16_t timeout;
	uint16_t min_delta;
	uint16_t log_length;
};

class WDTDevice : public Device
{
protected:
	bool active;
	uint16_t timeout;
	uint16_t min_delta;  // 0 - last reboot was caused by the watchdog
	uint32_t keep_alive_uptime;

	bool set_timeout(int32_t);
	bool send_status(SerialCommand *);
	bool send_log(SerialCommand *);

public:
	WDTDevice();
	bool run(SerialCommand *);
	void update();
};

inline void WDTDevice::update()
{
	int32_t delta;

	if (active) {
		delta = timeout - (sys_time.get_uptime() - keep_alive_uptime);
		if (min_delta > delta) min_delta = delta;

		if (delta <= 0) {
			active = false;
			min_delta = 0;

			PORTC |= _BV(PORTC1);  // RESET:HIGH
			_delay_ms(1000);
			PORTC &= ~_BV(PORTC1);  // RESET:LOW

			sys_log.write(sys_time.now(), LOG_RESET);
		}
	}
}

#endif /* WDTDEVICE_H_ */
