/*
  HardwareSerial.h - Hardware serial library for Wiring
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

  Modified 28 September 2010 by Mark Sproul
  Modified 14 August 2012 by Alarus
  Modified 3 December 2013 by Matthijs Kooijman
  Modified 26 April 2016 by Aleksandr Borisenko to make it compile without arduino core.
*/

#ifndef HARDWARESERIAL_H_
#define HARDWARESERIAL_H_

#include <stddef.h>
#include <string.h>
#include <inttypes.h>
#include <avr/interrupt.h>


// Must be <= 256 because uint8_t
// Actual buffer size N-1 because buffer_head == buffer_tail it's an empty buffer
#define SERIAL_TX_BUFFER_SIZE 128
#define SERIAL_RX_BUFFER_SIZE 128

#ifndef _BV
#define _BV(bit) (1 << (bit))
#endif

class HardwareSerial
{
protected:
	bool _written;

	volatile uint8_t _rx_buffer_head;
	volatile uint8_t _rx_buffer_tail;
	volatile uint8_t _tx_buffer_head;
	volatile uint8_t _tx_buffer_tail;

	unsigned char _rx_buffer[SERIAL_RX_BUFFER_SIZE];
	unsigned char _tx_buffer[SERIAL_TX_BUFFER_SIZE];

public:
	HardwareSerial();
	void begin(unsigned long);
	void end(void);
	void flush(void);
	size_t available(void);
	unsigned char read(void);
	size_t write(unsigned char c);
	size_t write(const char *str);
	size_t write(const unsigned char *buffer, size_t size);

	// Interrupt handlers
	inline void _rx_complete_irq(void);
	inline void _tx_udr_empty_irq(void);
};

inline size_t HardwareSerial::available(void)
{
	return (SERIAL_RX_BUFFER_SIZE + _rx_buffer_head - _rx_buffer_tail) % SERIAL_RX_BUFFER_SIZE;
}

inline unsigned char HardwareSerial::read(void)
{
	if (_rx_buffer_head != _rx_buffer_tail) {
		unsigned char c = _rx_buffer[_rx_buffer_tail];
		_rx_buffer_tail = (_rx_buffer_tail + 1) % SERIAL_RX_BUFFER_SIZE;
		return c;
	}
	return -1; // There is no way to return the error status here
}

inline size_t HardwareSerial::write(const char *str) {
	if (str == NULL) return 0;
	return write((const unsigned char *)str, strlen(str));
}

inline size_t HardwareSerial::write(const unsigned char *buffer, size_t size)
{
	size_t n = 0;
	while (size--) {
		if (write(*buffer++)) n++;
		else break;
	}
	return n;
}

inline size_t HardwareSerial::write(unsigned char c)
{
	_written = true;

	if (_tx_buffer_head == _tx_buffer_tail && bit_is_set(UCSR0A, UDRE0)) {
		UDR0 = c;
		UCSR0A |= _BV(TXC0);
		return 1;
	}
	uint8_t i = (_tx_buffer_head + 1) % SERIAL_TX_BUFFER_SIZE;

	// Buffer is full, waiting for the interrupt handler to empty it a bit
	while (i == _tx_buffer_tail) {
		if (bit_is_clear(SREG, SREG_I)) {
			// Interrupts are disabled, so poll the DR empty flag
			if(bit_is_set(UCSR0A, UDRE0)) _tx_udr_empty_irq();
		}
	}

	_tx_buffer[_tx_buffer_head] = c;
	_tx_buffer_head = i;

	UCSR0B |= _BV(UDRIE0);
	return 1;
}

inline void HardwareSerial::_tx_udr_empty_irq(void)
{
	unsigned char c = _tx_buffer[_tx_buffer_tail];
	_tx_buffer_tail = (_tx_buffer_tail + 1) % SERIAL_TX_BUFFER_SIZE;

	// Data Buffer Register
	UDR0 = c;
	UCSR0A |= _BV(TXC0);

	if (_tx_buffer_head == _tx_buffer_tail) {
		// Buffer empty, so disable interrupts
		UCSR0B &= ~_BV(UDRIE0);
	}
}

inline void HardwareSerial::_rx_complete_irq(void)
{
	// UCSR0A:UPE0 - USART Parity Error (No parity 8N1)
	unsigned char c = UDR0;
	uint8_t i = (_rx_buffer_head + 1) % SERIAL_RX_BUFFER_SIZE;

	if (i != _rx_buffer_tail) {
		_rx_buffer[_rx_buffer_head] = c;
		_rx_buffer_head = i;
	}
}

extern HardwareSerial sys_serial;

#endif /* HARDWARESERIAL_H_ */
