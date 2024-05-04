/*
  TSS.cpp - Hardware serial library for Wiring
  Copyright (c) 2006 Nicholas Zambetti.All right reserved.

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
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "Arduino.h"
#include "wiring_private.h"

#include "tss.h"
extern "C"{

  #ifndef SOFT_TX_ONLY
    tss_soft_ring_buffer TSS_rx_buffer = {{ 0 }, 0, 0};
    ISR(SOFTSERIAL_vect) {
      register uint8_t DelayCount asm ("r21");
      DelayCount = Serial._delayCount;
      // 4 clocks to get to the interrupt vector, per "Interrupt Response Time" section of datasheet, plus 2 for the rjmp itself
      // then there's the Prologue; push r0, push r1, in r0, sreg, push r0, push some register (ch) push r21 = 11 clocks total 17
      uint8_t ch;
      __asm__ __volatile__ (
        "ldi %[rxch], 0x80"        "\n\t"   // We want to start with 0b1000000 in the register that we're putting the received bits. We
        "clc"                      "\n\t"   // clear carry, we may then set it again. uartDelayTSS DOES NOT CHANGE CARRY because it uses use DEC decrement the number...
        "rcall uartDelayTSS"          "\n\t"   // delay for 1/4th of a bit period. this, combined with the prologue, 2 cycles to lds the delayCount into r21, and  2 clocks above (ldi, clc)
    //                                      // we hope will bring us to about the middle of the start bit. 28 + 3*_delayCount should be approx (actual bit time) / 2
    //  "_rxstart:" //label caused problems
        "rcall uartDelayTSS"          "\n\t"   // Wait 0.25 bit period
        "rcall uartDelayTSS"          "\n\t"   // Wait 0.25 bit period
        "rcall uartDelayTSS"          "\n\t"   // Wait 0.25 bit period
        "rcall uartDelayTSS"          "\n\t"   // Wait 0.25 bit period
        "sbic %[pin], %[rxbit]"    "\n\t"   // 1 Skip the SEC below if the bit is clear
        "sec"                      "\n\t"   // 2 Set the carry flag so we can shift it in
        "ror %[rxch]"              "\n\t"   // 3 This clears the carry bit UNTIL it shifts out the bit from the 0x80, which indicates that we got the whole byte and are done.
        "brcc .-16"                "\n\t"   // 5 clock total. Assuming carry is clear, we go back to the delay.
        //                                  // total (4 * (3 + 4 + 3 * _delayCount) + 5) * 8 + (17 + 4 + 3 + 4 + 3 * delayCount) -1 (brcc not branching last bit) clock cycles have gone by since the falling edge of the start bit.
        //                                  // (4 * (3 + 4 + 3 * _delayCount) + 5) * 8 + (17 + 4 + 3 + 4 + 3 * delayCount) -1  =  23 + 160 + 224 + 33 * 3 * _delayCount  = 447 + 99 * _delayCount
        //                                  // We sampled the final bit 4 clocks before that, at 443+99*_delayTime / (actual bit time in clocks) must be between 8 and 9 or we received garbage.
        // "sbis %[pin], %[rxbit]"   "\n\t" // check the port input bit itself  and wait until it is a 1 (meaning no further transitions and we're on either a 1 as last bit or the stop bit)
        // "rjmp .-4"                "\n\t" //
        // Wait - do we need this? The interrupt happens on the rising edge of ACO (falling edge of RX. If we're in the last bit, it's either a 1 or a 0. If it's a 1, then there are no further transitions)
        // and no reason wait before storing the character and exiting the interrupt. If it's a 0, the only edge is a rising edge on RX, which is a falling edge on ACO, which doesn't trigger the ISR.
        // No, we don't! And the faster we gtfo of the interrupt the faster the maximum baud rate and the better overall app performance, not to mention saving 2 words of flash.
        : [rxch]"=r" (ch)
        : [pin] "I" (_SFR_IO_ADDR(SOFTSERIAL_PIN)),
          [rxbit] "I" (SOFTSERIAL_RXBIT),
          [delaycount] "d" (DelayCount)
        : "r0"
      );

      uint8_t i = (uint8_t)(TSS_rx_buffer.head + 1) & (SERIAL_BUFFER_SIZE-1); //lds, andi

      // if we should be storing the received character into the location
      // just before the tail (meaning that the head would advance to the
      // current location of the tail), we're about to overflow the buffer
      // and so we don't write the character or advance the head.
      if (i != TSS_rx_buffer.tail) { // lds, cp, breq
        TSS_rx_buffer.buffer[TSS_rx_buffer.head] = ch; // ldi, ldi, add, adc, st
        TSS_rx_buffer.head = i; // sts
      }
      // total 2 + (2+1) + (2 + 1 + 1) + (1 + 1 + 1 + 1 + 2) = 2 + 3 + 4 + 6 = 15 clocks from sampling last bit
      // epilogue = prologue only with pop's instead of pushes, same time as prologue on classic AVRs = 11 clocks
      // reti 4 clocks
      // total 15 + 11 + 4 = 30 clocks from end of asm to end of ISR. which is 447 + 99 * _delayCount clocks from it's start
      // 477 + 99 * _delayCount total.
      // the falling edge of the next byte's start bit must not occur during this time, or we will get garbage for the next byte
      // (actual byte duration in clocks) < 477 * 99 * delayCount, or (actual bit time in clocks ) < 34 (30 + the 4 clocks from sample time to end of ISR) worst case assuming we're barely catching end of last bit.
    }

  #endif


  void uartDelayTSS() {
    __asm__ __volatile__ (
    "uartDelayTSS:"              "\n\t" // We rely on this being set up by the tx and rx routines, that way it doesn't have to ldi anything
      "mov  r0, r21"          "\n\t" // because I don't think it would be set up correctly if we tried to pass it in a constraint from here.
      "dec  r0"               "\n\t" // prev. line set up __temp_reg__ with _delayCount. decrement it
      "brne .-4"              "\n\t" // and loop if it's not 0. Total loop time including mov is 3 * _delayCount, the mov offset by the shorter brne not taken at end
      "ret"                   "\n\t" // 4 clocks for the return, and it took 3 to rcall - total 7 + 3*_delayCount.
      ::
    );
  }
}
#if defined(SOFT_TX_ONLY)
  TSS::TSS() {
    _txmask   = _BV(SOFTSERIAL_TXBIT);
    _delayCount = 0;
  }
#else
  TSS::TSS(tss_soft_ring_buffer *TSS_rx_buffer) {
    _TSS_rx_buffer = TSS_rx_buffer;

    _txmask   = _BV(SOFTSERIAL_TXBIT);

    _delayCount = 0;
  }
#endif

void TSS::setTxBit(uint8_t txbit) {
  _txmask   = _BV(txbit);
}

void TSS::begin(long baud) {
  long tempDelay = (((F_CPU/baud) - 39) / 12);
  if ((tempDelay > 255) || (tempDelay <= 0)) {
    return; //Cannot start - baud rate out of range.
  }
  _delayCount = (uint8_t)tempDelay;
  #ifndef SOFT_TX_ONLY
    // These should compile to cbi and sbi - everything is compile time known.
    SOFTSERIAL_DDR    &=  ~(1 << SOFTSERIAL_RXBIT);  // set RX to an input
    SOFTSERIAL_PORT   |=   (1 << SOFTSERIAL_RXBIT);  // enable pullup on RX pin - to prevent accidental interrupt triggers.
    PCMSK1 |= 1 << SOFTSERIAL_RXBIT;
    GIMSK |= 1 << PCIE1;  //enable PCINT1
    
  #endif
  uint8_t oldsreg      =   SREG;    //  These are NOT going to get compiled to cbi/sbi as _txmask is not compile time known.
  cli();                            //  so we need to protect this.
  SOFTSERIAL_DDR      |=   _txmask; //  set TX to an output.
  SOFTSERIAL_PORT     |=   _txmask; //  set TX pin high
  SREG                 =   oldsreg; //  restore SREG.
}

void TSS::end() {
  #ifndef SOFT_TX_ONLY
    PCMSK1 &= ~(1 << SOFTSERIAL_RXBIT);
    _TSS_rx_buffer->head = _TSS_rx_buffer->tail;
  #endif
  _delayCount = 0;
}

int TSS::available(void) {
  #ifndef SOFT_TX_ONLY
    if (_delayCount) {
      return (uint8_t)(SERIAL_BUFFER_SIZE + _TSS_rx_buffer->head - _TSS_rx_buffer->tail) & (SERIAL_BUFFER_SIZE-1);
    }
  #endif
  return 0;
}


int TSS::peek(void) {
  #ifndef SOFT_TX_ONLY
    if (_TSS_rx_buffer->head == _TSS_rx_buffer->tail) {
      return -1;
    } else {
      return _TSS_rx_buffer->buffer[_TSS_rx_buffer->tail];
    }
  #else
    return -1;
  #endif
}

int TSS::read(void) {
  #ifndef SOFT_TX_ONLY
    // if the head isn't ahead of the tail, we don't have any characters
    if (_TSS_rx_buffer->head == _TSS_rx_buffer->tail || _begun != 0) {
      return -1;
    } else {
      uint8_t c = _TSS_rx_buffer->buffer[_TSS_rx_buffer->tail];
      _TSS_rx_buffer->tail = (uint8_t)(_TSS_rx_buffer->tail + 1) & (SERIAL_BUFFER_SIZE-1);
      return c;
    }
  #else
    return -1;
  #endif
}

size_t TSS::write(uint8_t ch) {
  register uint8_t DelayCount asm ("r21");
  DelayCount = Serial._delayCount;
  if (!_delayCount) {
    return 0;
  }
  __asm__ __volatile__ (
    "in r1, 0x3F"                 "\n\t" // we don't use the known zero in this routine, so we stash the SREG there.
    //                                   // We can't use r0 for this, because uartDelayTSS uses r0 as it's delay counter.
    "cli"                         "\n\t" // disable interrupts if they're not disabled already.
    "ldi r20, 10"                 "\n\t" //
    "in r19, %[uartPort]"         "\n\t" // load the current value of the TX PORT register.
    "or r19, %[txmask]"           "\n\t" // there's our pattern for a 1. one hopes that this line is unnecessary since serial is IDLE HIGH.
    "mov r18,r19"                 "\n\t" // copy it to the zero bit...
    "eor r18, %[txmask]"          "\n\t" // xor with the txmask, which has all but 1 bit set, a bit which we know is set in the destination register. 4 instructions to prepare the 0 and 1 bit patterns.
    "com %[ch]"                   "\n\t" // Invert the bits because we will be shifting in 0's, and the stop bit has to be a 1, so a 0 in carry must output a "1" bit pattern.
    "sec"                         "\n\t" // clear the carry bit - the start bit is a 0 bit, so that's what we want to generate
  //"_txstart:"                          // labels cause problems with some optimization options which try to inline this.
  //                                     // running total of clocks so far this bit after executing this line is in left column.
    "brcc .+4"  /* _txpart2 */    "\n\t" //  1      if we have a 0 in carry bit, we want to output the 1 pattern stored in r19 so skip 2 insn's
    "out %[uartPort], r18"        "\n\t" //  2      output a zero bit (clock tally is for the case when branch is not taken)
    "rjmp .+4"                    "\n\t" //  4      jump over the two instructions for outputting a 1 bringing us to the delay part.
  //"_txpart2:"                          //  2      used with brcs with branch (we're starting from clock tally of 2 because we took the branch)
    "out %[uartPort], r19"        "\n\t" //  3      output a 1 bit
    "nop"                         "\n\t" //  4      pad the 1-bit case with a nop so that the two paths have equal running time.
    "rcall uartDelayTSS"             "\n\t" // then we do the 4 quarter-bit delays. rcall is okay here, because we don't support any 16k parts. The 167 and 1634 have HW serial.
    "rcall uartDelayTSS"             "\n\t" // rcall = 3 clocks, and the return = 4. The body of uartDelayTSS takes 1 clock for the mov
    "rcall uartDelayTSS"             "\n\t" // plus 3 * _delayCount, minus 1 on the last loop since branch not taken, or 3x _delayCount
    "rcall uartDelayTSS"             "\n\t" // that done, we'll rightshift the bit and decrement the count
    "lsr %[ch]"                   "\n\t" //  1 - shift the output character right one place, shifting in a 0 on the left. Shifted bit is in carry
    "dec r20"                     "\n\t" //  1 - decrement count
    "brne .-24"  /* to _txstart */"\n\t" //  2 total 4; Loop runtime per bit is hence 4 + 4 + 28 + 12*delaycount
    "out 0x3F, r1"                "\n\t" // restore SREG turning interrupts back on if they were on before
    "eor r1, r1"                  "\n\t" // and clear r1 since compiler expects it to always contain 0.
    : [ch] "+r" (ch) // this is a read-write variable - as the sending process is destructive.
    : [uartPort] "I" (_SFR_IO_ADDR(SOFTSERIAL_PORT)),
      [txmask] "r" ((uint8_t) _txmask),
      [delaycount] "d" (DelayCount) // we never read this, but we need to make sure it's in r21 because delayCount expects it there.
    : "r18","r19","r20"
    /* zero bit pattern, one bit pattern, count - all are call-clobbered, and we don't touch r24, which will contain ch when we start,
    and get replaced with 1 after this block to return 1, thus these clobbers should result in no additional code. */
  );
  /*  Time per bit is 8 + 4 (3 (rcall) + 4)) + 4 * uartDelayTSS
   *  or 36 + 4*uartDelayTSS. uartDelayTSS = is lds, mov (3) + (dec brne) * loopcount.
   *  so we have 48 + 12*loopcount clocks per bit.
   *  For RX is 45 + 12*loopcount
   */
  return 1;
}
void TSS::printHex(const uint8_t b) {
    char x = (b >> 4) | '0';
    if (x > '9')
      x += 7;
    write(x);
    x = (b & 0x0F) | '0';
    if (x > '9')
      x += 7;
    write(x);
  }

  void TSS::printHex(const uint16_t w, bool swaporder) {
    uint8_t *ptr = (uint8_t *) &w;
    if (swaporder) {
      printHex(*(ptr++));
      printHex(*(ptr));
    } else {
      printHex(*(ptr + 1));
      printHex(*(ptr));
    }
  }

  void TSS::printHex(const uint32_t l, bool swaporder) {
    uint8_t *ptr = (uint8_t *) &l;
    if (swaporder) {
      printHex(*(ptr++));
      printHex(*(ptr++));
      printHex(*(ptr++));
      printHex(*(ptr));
    } else {
      ptr+=3;
      printHex(*(ptr--));
      printHex(*(ptr--));
      printHex(*(ptr--));
      printHex(*(ptr));
    }
  }

  uint8_t * TSS::printHex(uint8_t* p, uint8_t len, char sep) {
    for (byte i = 0; i < len; i++) {
      if (sep && i) write(sep);
      printHex(*p++);
    }
    println();
    return p;
  }

  uint16_t * TSS::printHex(uint16_t* p, uint8_t len, char sep, bool swaporder) {
    for (byte i = 0; i < len; i++) {
      if (sep && i) write(sep);
      printHex(*p++, swaporder);
    }
    println();
    return p;
  }
void TSS::flush() {
  ;
}

TSS::operator bool() {
  return !!_delayCount;
}

#if defined(SOFT_TX_ONLY)
  TSS TinySer;
#else
  TSS TinySer(&TSS_rx_buffer);
#endif
