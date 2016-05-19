// The main idea was found in the arduino ide source tree, then heavily modified
// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#include <avr/io.h>
#include <inttypes.h>
#include <stdio.h>
#include <avr/pgmspace.h>
#include <avr/interrupt.h>
#include "Time.h"

Time::Time() : timer0_millis(0), timer0_fract(0), time(0), prev_us_time(0), 
	uptime(0), prev_us_uptime(0), status(TIME_NOT_SET)
{
	// CLKio/64 prescaler
	TCCR0B |= (_BV(CS01) | _BV(CS00));

	cli();
	// Overflow Interrupt Enable
	TIMSK0 |= _BV(TOIE0);
	sei();
}

int32_t Time::now()
{
	uint32_t delta = (millis() - prev_us_time) / 1000;  // Ex: 1000 - 4294967290 = 1006
	if ( delta > 0 ) {
		time += delta;
		prev_us_time += delta * 1000;
	}

	delta = (millis() - prev_us_uptime) / 1000;
	if ( delta > 0 ) {
		uptime += delta;
		prev_us_uptime += delta * 1000;
	}

	return time;
}

int32_t Time::set_time(int32_t t)
{
	int32_t offset = t - time;

	time = t;
	status = TIME_SET;
	prev_us_time = millis();

	return offset;
}

Time sys_time;

ISR(TIMER0_OVF_vect)
{
	sys_time._overflow_interrupt_irq();
}
