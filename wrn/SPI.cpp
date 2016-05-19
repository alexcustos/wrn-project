/*
 * Copyright (c) 2010 by Cristian Maglie <c.maglie@arduino.cc>
 * Copyright (c) 2014 by Paul Stoffregen <paul@pjrc.com> (Transaction API)
 * Copyright (c) 2014 by Matthijs Kooijman <matthijs@stdin.nl> (SPISettings AVR)
 * Copyright (c) 2014 by Andrew J. Kroll <xxxajk@gmail.com> (atomicity fixes)
 * Copyright (c) 2016 by Aleksandr Borisenko (make it compile without arduino core)
 * NOTE: Only the ATmega328P is supported.
 *
 * SPI Master library for arduino.
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of either the GNU General Public License version 2
 * or the GNU Lesser General Public License version 2.1, both as
 * published by the Free Software Foundation.
 */

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include "SPI.h"

SPIClass SPI;

uint8_t SPIClass::initialized = 0;
uint8_t SPIClass::interruptMode = 0;
uint8_t SPIClass::interruptMask = 0;
uint8_t SPIClass::interruptSave = 0;

void SPIClass::begin()
{
	uint8_t sreg = SREG;
	cli();

	if (!initialized) {
		// When set as INPUT, the SS pin should be given as HIGH on as Master, and a LOW on a Slave.
		// When as an OUTPUT pin on the Master, the SS pin can be used as a GPIO pin.

		// SS: DDRB - Data Direction Register; IF: 0 - INPUT; 1 - OUTPUT
		// PORTB: Data Register; HIGH - enable the internal pull-up resistor
		if (!(DDRB & _BV(DDB2))) PORTB |= _BV(PORTB2);

		// SS: OUTPUT
		DDRB |= _BV(DDB2);

		// SPI Enable; Master Select
		SPCR |= (_BV(SPE) | _BV(MSTR));

		// MISO pin automatically overrides to INPUT.
		// SLK & MOSI: OUTPUT
		DDRB |= (_BV(DDB5) | _BV(DDB3));
	}
	initialized++; // reference count
	SREG = sreg;
}

void SPIClass::end() {
	uint8_t sreg = SREG;
	cli();

	// Decrease the reference counter
	if (initialized)
	initialized--;
	// If there are no more references disable SPI
	if (!initialized) {
		// SPI Disable
		SPCR &= ~_BV(SPE);
		interruptMode = 0;
	}
	SREG = sreg;
}

void SPIClass::usingInterrupt(uint8_t interruptNumber)
{
	uint8_t sreg = SREG;
	cli();

	switch (interruptNumber) {
	case 0:
		interruptMask |= _BV(INT0);
		break;
	case 1:
		interruptMask |= _BV(INT1);
		break;
	default:
		interruptMode = 2;
		break;
	}

	if (!interruptMode) interruptMode = 1;
	SREG = sreg;
}

void SPIClass::notUsingInterrupt(uint8_t interruptNumber)
{
	// Once in mode 2 we can't go back to 0 without a proper reference count
	if (interruptMode == 2)	return;
	uint8_t sreg = SREG;
	cli();

	switch (interruptNumber) {
	case 0:
		interruptMask &= ~_BV(INT0);
		break;
	case 1:
		interruptMask &= ~_BV(INT1);
		break;
	default:
		break;
	}

	if (!interruptMask)	interruptMode = 0;
	SREG = sreg;
}
