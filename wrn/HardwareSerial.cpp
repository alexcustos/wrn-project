/*
  HardwareSerial.cpp - Hardware serial library for Wiring
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Modified 23 November 2006 by David A. Mellis
  Modified 28 September 2010 by Mark Sproul
  Modified 14 August 2012 by Alarus
  Modified 3 December 2013 by Matthijs Kooijman
  Modified 26 April 2016 by Aleksandr Borisenko to make it compile without arduino core.
*/

#include <avr/io.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include "HardwareSerial.h"

HardwareSerial::HardwareSerial() : _written(false), _rx_buffer_head(0), _rx_buffer_tail(0), _tx_buffer_head(0), _tx_buffer_tail(0)
{
}

void HardwareSerial::begin(unsigned long baud)
{
	uint16_t baud_setting = F_CPU / 8 / baud - 1;

	// UBRR 12-bit register
	if (baud_setting > 4095) {
		// USART Control and Status Register A
		UCSR0A = 0;
		baud_setting = F_CPU / 16 / baud - 1;
	} else {
		UCSR0A = _BV(U2X0);
	}

	// USART Baud Rate Registers
	UBRR0H = baud_setting >> 8;
	UBRR0L = baud_setting;

	// Asynchronous USART; Parity Disabled; 1-Stop Bit; 8-bit Character Size
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);

	// Receiver Enable; Transmitter Enable
	UCSR0B = _BV(RXEN0) | _BV(TXEN0);

	cli();
	// RX Complete Interrupt Enable
	UCSR0B |= _BV(RXCIE0);
	sei();

	_written = false;
}

void HardwareSerial::end(void)
{
	flush();
	UCSR0B = 0; // Initial Value
	_rx_buffer_head = _rx_buffer_tail;
}

void HardwareSerial::flush()
{
	// No way to force the TXC (transmit complete) bit to 1 during initialization
	if (!_written) return;

	// UDRIE0 - USART Data Register Empty Interrupt Enable
	// TXC0 - USART Transmit Complete
	// UDRE0 - USART Data Register Empty
	while (bit_is_set(UCSR0B, UDRIE0) || bit_is_clear(UCSR0A, TXC0)) {
		if (bit_is_clear(SREG, SREG_I) && bit_is_set(UCSR0B, UDRIE0)) {
			// Interrupts are globally disabled, so poll the DR empty flag
			if (bit_is_set(UCSR0A, UDRE0)) _tx_udr_empty_irq();
		}
	}
}

HardwareSerial sys_serial;

ISR(USART_RX_vect)
{
	sys_serial._rx_complete_irq();
}

ISR(USART_UDRE_vect)
{
	sys_serial._tx_udr_empty_irq();
}
