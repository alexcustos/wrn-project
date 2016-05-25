// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#define DEBUG_CMD_DELAY 1000
#define RF24_CHANEL 62 // uint8_t
#define WDT_BITS_PER_CYCLE 16


#include <avr/io.h>
#include <avr/wdt.h>
#include <stdio.h>
#include <util/delay.h>
#include <inttypes.h>
#include <string.h>
#include "HardwareSerial.h"
#include "Time.h"
#include "SerialCommand.h"
#include "Devices.h"
#include "WDTDevice.h"
#include "RNGDevice.h"
#include "SPI.h"
#include <RF24Network.h>
#include <RF24.h>
#include "EepromLog.h"
#include "main.h"

SerialCommand cmd(&sys_serial);
CommonDevice common_device;
WDTDevice wdt_device;
RNGDevice rng_device;
NRFDevice nrf_device;

// const uint16_t sensor_node = 1;
const uint16_t rx_node = 0;

RF24 radio;
RF24Network network(&radio);

int serial_putc(char c, FILE *)
{
	// 0 - if the output was successful, and a nonzero value if the character could not be sent
	return (sys_serial.write(c) != 1);
}

void blink_once(void)
{
	PORTC |= _BV(PORTC0);
	_delay_ms(10);
	PORTC &= ~_BV(PORTC0);
}

int main(void)
{
	bool cmd_ok = false;

	wdt_enable(WDTO_8S);

	// ADC enable; prescaler: 128 (156 KHz @ 20MHz)
	ADCSRA |= (_BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0));
	DDRC |= _BV(DDC0);  // LED:OUTPUT

	// NLOCK: locked
	DDRC |= _BV(DDC2);  // OUTPUT
	PORTC &= ~_BV(PORTC2);  // LOW

#ifdef DEBUG
	int m = DEBUG_CMD_DELAY;  // ms
	fdevopen(&serial_putc, 0);  // open the stdout and stderr streams
#endif

	sys_serial.begin(57600);
	IF_DEBUG(printf_P(PSTR("BOOT\r\n")));

	// initiate a sync with the daemon after boot
	// there is a 1:65536 chance that sync will not be lost
	// the daemon will initialize the device by the command
	cmd.set(CMD_COMMON, COMMON_RESET, 0, 0);
	common_device.confirm_boot(&cmd);
	//cmd.reset();  // look to the SerialCommand::read

	sys_log.begin();
	SPI.begin();
	if (radio.begin()) {
		 network.begin(RF24_CHANEL, rx_node);
		 nrf_device.setup(&network);
	}

	while(true)
	{
		wdt_reset();

		// CMD_NRF_FORWARD
		if (nrf_device.read(&cmd)) {
			if (!nrf_device.run(&cmd)) cmd.send_header(-1);
			//cmd.reset();
		}

		// CMD_RNG_SEND
		for (int i = 0; i < WDT_BITS_PER_CYCLE; i++) {
			if (rng_device.read(&cmd)) {
				if (!rng_device.run(&cmd)) cmd.send_header(-1);
				//cmd.reset();
			}
		}

		// watchdog trigger
		wdt_device.update();

#ifdef DEBUG
		if (--m <= 0) {
			m = DEBUG_CMD_DELAY;
			//cmd.set(CMD_COMMON, 1, 1460792071, 0); // look to the SerialCommand::read
			//sys_log.write(sys_time.now(), LOG_BOOT);
		}
		//_delay_ms(1);
#endif

		// serial commands
		if (cmd.read()) {
#ifdef DEBUG
			blink_once();
#endif
			cmd_ok = false;
			switch (cmd.get_type()) {
				case CMD_COMMON:
					cmd_ok = common_device.run(&cmd);
					break;
				case CMD_WDT:
					cmd_ok = wdt_device.run(&cmd);
					break;
				case CMD_RNG:
					cmd_ok = rng_device.run(&cmd);
					break;
				case CMD_NRF:
					cmd_ok = nrf_device.run(&cmd);
					break;
				default:
					break;
			}
			if (!cmd_ok) cmd.send_header(-1);
			//cmd.reset();
		}
	} // while(true)

	sys_serial.end();
	return 0;
} // main
