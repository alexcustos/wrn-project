# WRN Project

WRN is 3-in-1 device for RS232 serial port was designed to solve some standard problems of a home server.

## The board based on the ATmega328P and consist of:

* The hardware watchdog timer
* True random number generator (based on avalanche noise)
* Low power 2.4GHz transceiver (nRF24L01+)

## The project consist of the following components:

* The schematic and the PCB (KiCAD project)
* The firmware (AtmelStudio project)
* Linux daemon, watchdog driver and the installation for the Gentoo (Makefile)

The project depends on:
* [alexcustos/RF24](https://github.com/alexcustos/RF24)
* [alexcustos/RF24Network](https://github.com/alexcustos/RF24Network)

Here's an article in russian with more details: 
