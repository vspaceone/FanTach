/*
 SoftwareSerial.c (formerly SoftwareSerial.cpp, formerly NewSoftSerial.c) -
 Single-instance software serial library for ATtiny, modified from Arduino SoftwareSerial.
 -- Interrupt-driven receive and other improvements by ladyada
 (http://ladyada.net)
 -- Tuning, circular buffer, derivation from class Print/Stream,
 multi-instance support, porting to 8MHz processors,
 various optimizations, PROGMEM delay tables, inverse logic and
 direct port writing by Mikal Hart (http://www.arduiniana.org)
 -- Pin change interrupt macros by Paul Stoffregen (http://www.pjrc.com)
 -- 20MHz processor support by Garrett Mace (http://www.macetech.com)
 -- ATmega1280/2560 support by Brett Hagman (http://www.roguerobotics.com/)
 -- Port to ATtiny84A / C by Michael Shimniok (http://www.bot-thoughts.com/)

Notes on the ATtiny84A port. To save space I've:
 - Converted back to C
 - Removed the ability to have mulitple serial ports,
 - Hardcoded the RX pin to PA0 and TX pin to PA1
 - Using & mask versus modulo (%)
 - A few other tweaks to get the code near 1k
More notes:
 - Converted from Arduinoish library stuff (pins etc)
 - Error is too high at 57600 (0.64%) and 115200 (2.12%)
 - Ok at 38400 and lower.
 - Still working out how to prevent missing bits when writing characters

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

*/

//
// Defines
//
#define true 1
#define false 0
#define HIGH 1
#define LOW 0

// 
// Includes
// 
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "SoftwareSerial.h"
//
// Globals
//


//[H3] hardcoded baudrate=1200bps to save space

#if F_CPU == 1000000
/*	//  baud    rxcenter    rxintra    rxstop  tx
	{ 4800,     14,        28,       27,    27,    },
	{ 2400,     28,        56,       56,    56,    },
	{ 1200,     56,        118,      118,   118,   },
	{ 300,      224,       475,      475,   475,   },*/

uint16_t _rx_delay_centering = 56;
uint16_t _rx_delay_intrabit = 118;
uint16_t _rx_delay_stopbit = 118;
uint16_t _tx_delay = 118;
const int XMIT_START_ADJUSTMENT = 3;

#elif F_CPU == 8000000
/*	//  baud    rxcenter    rxintra    rxstop  tx
	{ 4800,     110,        233,       233,    230,    },
	{ 1200,     467,        948,       948,    945,    },
	{ 2400,     229,        472,       472,    469,    },
	{ 300,      1895,       3805,      3805,   3802,   },*/

uint16_t _rx_delay_centering = 229;
uint16_t _rx_delay_intrabit = 472;
uint16_t _rx_delay_stopbit = 472;
uint16_t _tx_delay = 469;
const int XMIT_START_ADJUSTMENT = 4;

#elif F_CPU == 16000000
static const DELAY_TABLE PROGMEM table[] =
/*	//  baud    rxcenter   rxintra    rxstop    tx
	{ 4800,     233,       474,       474,      471,   },
	{ 2400,     471,       950,       950,      947,   },
	{ 1200,     947,       1902,      1902,     1899,  },
	{ 300,      3804,      7617,      7617,     7614,  }, */

uint16_t _rx_delay_centering = 947;
uint16_t _rx_delay_intrabit = 1902;
uint16_t _rx_delay_stopbit = 1902;
uint16_t _tx_delay = 1899;
const int XMIT_START_ADJUSTMENT = 5;
#else
#error Use 8MHz, 4MHz, or 1MHz clock
#endif

uint16_t _buffer_overflow = false;

// static data
static char _receive_buffer[_SS_MAX_RX_BUFF];
static volatile uint8_t _receive_buffer_tail;
static volatile uint8_t _receive_buffer_head;
//static SoftwareSerial *active_object;

// private methods
#define rx_pin_read() (SERPIN & (1<<RXPIN))

// private static method for timing
static inline void tunedDelay(uint16_t delay);


//
// Private methods
//

/* static */
inline void tunedDelay(uint16_t delay) {
	uint8_t tmp = 0;

	asm volatile("sbiw    %0, 0x01 \n\t"
			"ldi %1, 0xFF \n\t"
			"cpi %A0, 0xFF \n\t"
			"cpc %B0, %1 \n\t"
			"brne .-10 \n\t"
			: "+w" (delay), "+a" (tmp)
			: "0" (delay)
	);
}


//
// Public methods
//

void softSerialBegin() {
	_receive_buffer_head = _receive_buffer_tail = 0;
	_buffer_overflow = false;
	SERDDR |= (1<<TXPIN); // set TX for output
	SERPORT |= (1<<TXPIN); // assumes no inverse logic
	
	tunedDelay(_tx_delay);
	sei();
	return;

	// No valid rate found
	// Indicate an error
}

size_t softSerialWrite(uint8_t b) {
	if (_tx_delay == 0) {
		//setWriteError();
		return 0;
	}

	uint8_t oldSREG = SREG; // store interrupt flag
	cli();	// turn off interrupts for a clean txmit

	// Write the start bit
	SERPORT &= ~(1<<TXPIN); // tx pin low
	tunedDelay(_tx_delay + XMIT_START_ADJUSTMENT);

	// Write each of the 8 bits
	for (byte mask = 0x01; mask; mask <<= 1) {
		if (b & mask) // choose bit
			SERPORT |= (1<<TXPIN); // tx pin high, send 1
		else
			SERPORT &= ~(1<<TXPIN); // tx pin low, send 0

		tunedDelay(_tx_delay);
	}
	SERPORT |= (1<<TXPIN); // tx pin high, restore pin to natural state

	//sei();
	SREG = oldSREG; // turn interrupts back on
	tunedDelay(_tx_delay);

	return 1;
}
