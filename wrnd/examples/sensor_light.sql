-- Copyright (c) 2016 Aleksandr Borisenko
-- Distributed under the terms of the GNU General Public License v2

PRAGMA foreign_keys = ON;

/*
sqlite3 sensors.db " \
	SELECT sl.rowid, datetime(created, 'localtime'), uptime, vcc, tmp36, l.name, s.name \
	FROM sensor_light AS sl LEFT JOIN light_type AS l USING (light) LEFT JOIN stat_type AS s USING (stat) \
	ORDER BY sl.rowid DESC LIMIT 10"

struct nrf_light
{
	uint16_t id;
	uint32_t uptime;
	uint8_t light;
	int32_t vcc;
	int32_t tmp36;
	uint8_t stat;
};

enum light_t {
	LIGHT_ON,
	LIGHT_OFF,
	LIGHT_FUZZY,
	LIGHT_UNKNOWN
};

enum stat_t {
	STAT_OK,
	STAT_BOOT,
	STAT_LIGHT,
	STAT_FUZZY,
	STAT_UNKNOWN
};
*/

CREATE TABLE sensor_light (
	created DATETIME DEFAULT CURRENT_TIMESTAMP NOT NULL,
	id INTEGER NOT NULL,
	uptime INTEGER NOT NULL,
	light INTEGER DEFAULT 3 REFERENCES light_type(light) NOT NULL,
	vcc INTEGER NOT NULL,
	tmp36 INTEGER NOT NULL,
	stat INTEGER DEFAULT 4 REFERENCES stat_type(stat) NOT NULL
);
CREATE INDEX created_index ON sensor_light (created);
CREATE INDEX id_index ON sensor_light (id);
CREATE INDEX light_index ON sensor_light (light);
CREATE INDEX stat_index ON sensor_light (stat);

CREATE TABLE light_type (
	light INTEGER PRIMARY KEY NOT NULL,
	name CHAR(16) NOT NULL
);
INSERT INTO light_type (light, name) VALUES (0, "ON");
INSERT INTO light_type (light, name) VALUES (1, "OFF");
INSERT INTO light_type (light, name) VALUES (2, "FUZZY");
INSERT INTO light_type (light, name) VALUES (3, "UNKNOWN");

CREATE TABLE stat_type (
	stat INTEGER PRIMARY KEY NOT NULL,
	name CHAR(16) NOT NULL
);
INSERT INTO stat_type (stat, name) VALUES (0, "OK");
INSERT INTO stat_type (stat, name) VALUES (1, "BOOT");
INSERT INTO stat_type (stat, name) VALUES (2, "LIGHT");
INSERT INTO stat_type (stat, name) VALUES (3, "FUZZY");
INSERT INTO stat_type (stat, name) VALUES (4, "UNKNOWN");
