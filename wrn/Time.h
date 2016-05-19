// The main idea was found in the arduino ide source tree, then heavily modified
// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#ifndef TIME_H_
#define TIME_H_

#include <inttypes.h>
#include <avr/interrupt.h>

#if (F_CPU == 16000000L) || (F_CPU == 8000000L)
// 1 + 24/1000 (3/125)
#define MICROSECONDS_PER_TIMER0_OVERFLOW (64 * 256 * 1000000L / F_CPU )
#define MILLIS_INC (MICROSECONDS_PER_TIMER0_OVERFLOW / 1000)
#define FRACT_INC ((MICROSECONDS_PER_TIMER0_OVERFLOW % 1000) >> 3)
#define FRACT_MAX (1000 >> 3)
#elif (F_CPU == 20000000L)
// 0 + 8192/10000 (512/625)
#define MILLIS_INC (0)
#define FRACT_INC (512)
#define FRACT_MAX (625)
#endif

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

enum TimeStatus {
	TIME_NOT_SET,
	TIME_SET
};

class Time
{
protected:
	volatile uint32_t timer0_millis;
	volatile uint16_t timer0_fract;

	int32_t time;
	uint32_t prev_us_time;
	uint32_t uptime;
	uint32_t prev_us_uptime;
	TimeStatus status;

public:
	Time();
	int32_t now();
	int32_t set_time(int32_t);
	uint32_t millis();
	TimeStatus get_status() { return status; };
	uint32_t get_uptime() { now(); return uptime; };

	// Interrupt handler
	void _overflow_interrupt_irq(void);
};

inline uint32_t Time::millis()
{
	uint32_t m;
	uint8_t sreg = SREG;

	cli();
	m = timer0_millis;
	SREG = sreg;

	return m;
}

inline void Time::_overflow_interrupt_irq(void)
{
	uint32_t m = timer0_millis;
	uint32_t f = timer0_fract;

	m += MILLIS_INC;
	f += FRACT_INC;
	if (f >= FRACT_MAX) {
		f -= FRACT_MAX;
		m += 1;
	}

	timer0_fract = f;
	timer0_millis = m;
}

extern Time sys_time;

#endif /* TIME_H_ */
