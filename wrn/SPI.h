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

#ifndef _SPI_H_INCLUDED
#define _SPI_H_INCLUDED

#include <avr/interrupt.h>
#include <stdlib.h>

// DORD (5)
#define LSBFIRST 0
#define MSBFIRST 1

// CPOL (3); CPHA (2)
#define SPI_MODE0 0x00
#define SPI_MODE1 0x04
#define SPI_MODE2 0x08
#define SPI_MODE3 0x0C

class SPISettings
{
public:
	SPISettings(uint32_t clock, uint8_t bitOrder, uint8_t dataMode)
	{
		init(clock, bitOrder, dataMode);
	}
	SPISettings()
	{
		init(4000000, MSBFIRST, SPI_MODE0);
	}
private:
	friend class SPIClass;
	uint8_t spcr;
	uint8_t spsr;

	void init(uint32_t clock, uint8_t bitOrder, uint8_t dataMode) __attribute__((__always_inline__))
	{
		// Note that this shows SPI2X inverted:
		// SPR1 SPR0 ~SPI2X Freq
		//   0    0     0   fosc/2
		//   0    0     1   fosc/4
		//   0    1     0   fosc/8
		//   0    1     1   fosc/16
		//   1    0     0   fosc/32
		//   1    0     1   fosc/64
		//   1    1     0   fosc/64
		//   1    1     1   fosc/128

		uint8_t clockDiv;

		if (clock >= F_CPU / 2) clockDiv = 0;
		else if (clock >= F_CPU / 4) clockDiv = 1;
		else if (clock >= F_CPU / 8) clockDiv = 2;
		else if (clock >= F_CPU / 16) clockDiv = 3;
		else if (clock >= F_CPU / 32) clockDiv = 4;
		else if (clock >= F_CPU / 64) clockDiv = 5;
		else clockDiv = 7;

		// Invert the SPI2X bit
		clockDiv ^= 0x1;

		// SPI Enable; Master Select;
		spcr = _BV(SPE) | _BV(MSTR) | ((bitOrder == LSBFIRST) ? _BV(DORD) : 0) |
			(dataMode & 0b00001100) | (clockDiv >> 1);
		spsr = clockDiv & 0b00000001;
	}
};

class SPIClass
{
public:
	static void begin();

	static void usingInterrupt(uint8_t interruptNumber);
	static void notUsingInterrupt(uint8_t interruptNumber);

	inline static void beginTransaction(SPISettings settings)
	{
		if (interruptMode > 0) {
			uint8_t sreg = SREG;
			cli();

			if (interruptMode == 1) {
				interruptSave = EIMSK;
				EIMSK &= ~interruptMask;
				SREG = sreg;
			} else interruptSave = sreg;
		}

		SPCR = settings.spcr;
		SPSR = settings.spsr;
	}

	inline static void endTransaction(void)
	{
		if (interruptMode > 0) {
			uint8_t sreg = SREG;
			cli();

			if (interruptMode == 1) {
				EIMSK = interruptSave;
				SREG = sreg;
			} else SREG = interruptSave;
		}
	}

	// Write to the SPI bus (MOSI pin) and also receive (MISO pin)
	inline static uint8_t transfer(uint8_t data)
	{
		SPDR = data;
		// Small delay that can prevent the wait loop. This gives about 10% more speed
		// asm volatile("nop");
		while (!(SPSR & _BV(SPIF)));
		return SPDR;
	}

	inline static void transfer(void *buf, size_t count)
	{
		if (count == 0) return;
		uint8_t *p = (uint8_t *)buf;

		SPDR = *p;
		while (--count > 0) {
			uint8_t out = *(p + 1);

			while (!(SPSR & _BV(SPIF)));
			uint8_t in = SPDR;
			SPDR = out;
			*p++ = in;
		}

		while (!(SPSR & _BV(SPIF)));
		*p = SPDR;
	}

	static void end();

private:
	static uint8_t initialized;
	static uint8_t interruptMode; // 0=none, 1=mask, 2=global
	static uint8_t interruptMask; // which interrupts to mask
	static uint8_t interruptSave; // temp storage, to restore state
};

extern SPIClass SPI;

#endif
