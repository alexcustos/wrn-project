// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#include <avr/io.h>
#include <util/delay.h>
#include "Utils.h"

uint32_t read_vcc()
{
	uint8_t low, high;
	uint32_t res;

	// REFS: 01 - reference AVCC with external capacitor at AREF pin
	// MUX: 1110 - input channel 1.1V (Vbg)
	ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);

	_delay_ms(1);  // ATmega328p datasheet do not mention it, but otherwise the first measure is inaccurate
	ADCSRA |= _BV(ADSC);  // ADC Start Conversion
	while (bit_is_set(ADCSRA, ADSC));  // When the conversion is complete, it returns to zero

	// ADC Data Register; ADCL must be read first, then ADCH
	low = ADCL;
	high = ADCH;
	res = (high << 8) | low;

	if (res > 0) res = 1125300L / res;  // calculate Vcc (in mV); 1125300 = 1.1 * 1023 * 1000
	else return 0;

	return res;  // Vcc in millivolts
}
