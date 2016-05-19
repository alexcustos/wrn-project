// Copyright (c) 2016 Aleksandr Borisenko
// Distributed under the terms of the GNU General Public License v2

#ifndef LOG_H_
#define LOG_H_

enum LogEvent {
	LOG_EMPTY = 0,
	LOG_BOOT,
	LOG_RESET
};

struct LogRecord
{
	int32_t time;
	uint8_t log_event;
};

class Log {
protected:
	int16_t size;
	int16_t log_begin;
	int16_t log_end;
	int16_t log_next;
	bool reverse;
	LogRecord record;

	void next();
	void prev();

public:
	Log();
	bool begin();
	LogRecord *read();
	void write(int32_t, uint8_t);
	void clean();
	uint16_t length();
	void set_reverse(bool);
	void set_limit(uint16_t);
};

extern Log sys_log;

#endif /* LOG_H_ */
