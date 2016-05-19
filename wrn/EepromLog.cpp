// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#include <stdlib.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include "Time.h"
#include "EepromLog.h"
#include "main.h"

Log::Log() : size(0), log_begin(0), log_end(0), log_next(0), reverse(false)
{
	record.time = 0;
	record.log_event = 0;
}

bool Log::begin()
{
	size_t sr = sizeof record;
	uint16_t end;
	size = (E2END / sr) * sr;
	log_begin = 0;
	log_end = 0;
	log_next = 0; // set to log_end after each iteration
	reverse = false;

	do {
		end = log_next; // the next write will be to marker
		next();
	} while (record.time > 0 && record.log_event > 0 && log_next != 0);
	if (record.time != 0 || record.log_event != 0) return false; // log is corrupted

	log_begin = log_next;
	next();  // looking at the next record
	log_end = end;
	if (record.time == 0 && record.log_event == 0) log_begin = 0;
	log_next = log_begin;
	IF_DEBUG(printf_P(PSTR("Log [Status] Size:%d, Length:%u; Begin:%d; End:%d; Next:%d\r\n"),
		size / sr - 1, length(), log_begin, log_end, log_next));

	return true;
}

void Log::next()
{
	size_t sr = sizeof record;
	eeprom_read_block((void *)&record, (const void *)log_next, sr);
	//IF_DEBUG(printf_P(PSTR("Log [Next] Pos:%d; Time:%ld; Event:%u\r\n"), log_next, record.time, record.log_event));
	log_next += sr;
	if (log_next >= size) log_next = 0;
}

void Log::prev()
{
	size_t sr = sizeof record;
	log_next -= sr;
	if (log_next < 0) log_next = size - sr;
	eeprom_read_block((void *)&record, (const void *)log_next, sr);
	//IF_DEBUG(printf_P(PSTR("Log [Prev] Pos:%d; Time:%ld; Event:%u\r\n"), log_next, record.time, record.log_event));
}

LogRecord *Log::read()
{
	if (reverse) {
		if (log_next == log_begin) {
			log_next = log_end;
			return NULL;
		}
		prev();
	} else {
		if (log_next == log_end) {
			log_next = log_begin;
			return NULL;
		}
		next();
	}
	return &record;
}

void Log::write(int32_t time, uint8_t log_event)
{
	size_t sr = sizeof record;

	record.time = time;
	record.log_event = log_event;

	eeprom_write_block((const void *)&record, (void *)log_end, sr);
	IF_DEBUG(printf_P(PSTR("Log [Write] Pos:%d; Time:%ld; Event:%u\r\n"), log_end, record.time, record.log_event));
	log_end += sr;
	if (log_end >= size) log_end = 0;

	// marker
	record.time = 0;
	record.log_event = 0;
	eeprom_write_block((const void *)&record, (void *)log_end, sr);
	if (log_begin == log_end) {
		log_begin += sr;
		if (log_begin >= size) log_begin = 0;
		log_next = reverse ? log_end : log_begin;
	}
}

void Log::clean()
{
	size_t sr = sizeof record;

	log_begin = 0;
	log_end = 0;
	log_next = 0;
	record.time = 0;
	record.log_event = 0;

	do {
		eeprom_write_block((const void *)&record, (void *)log_next, sr);
		log_next += sr;
		if (log_next >= size) log_next = 0;
	} while (log_next != 0);

	IF_DEBUG(printf_P(PSTR("Log [Clean] Done\r\n")));  // 3.4 sec
}

uint16_t Log::length()
{
	return ((size + log_end - log_begin) % size) / sizeof record;
}

void Log::set_reverse(bool r)
{
	reverse = r;
	log_next = reverse ? log_end : log_begin;
}

void Log::set_limit(uint16_t limit)
{
	if (limit == 0 || length() <= limit) return;

	size_t sr = sizeof record;

	if (reverse) log_next = log_begin + limit * sr;
	else log_next = log_end - limit * sr;

	log_next = (size + log_next) % size;
}

Log sys_log;
