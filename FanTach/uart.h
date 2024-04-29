/*  Software ("bit-bang") UART Transmitter (8 data bits, 1 stop bit, no parity)
    for Attiny24A/44A/84A using the internal 8MHz oscillator as clock source

    (c) 2018 Marcel Meyer-Garcia
    modified by H3 (2024)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

 /* NOTE: since the internal 8MHz oscillator is not very accurate, the value for OCR0A can be tuned
    to achieve the desired baud rate (nominal value is 103)
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// change these to use another pin
#define TX_PORT PORTB
#define TX_PIN PB0
#define TX_DDR DDRB
#define TX_DDR_PIN DDB0

volatile uint16_t tx_shift_reg = 0;


void UART_tx(char character) {
  uint16_t local_tx_shift_reg = tx_shift_reg;
  //if sending the previous character is not yet finished, return
  //transmission is finished when tx_shift_reg == 0
  if (local_tx_shift_reg) { return; }
  //fill the TX shift register witch the character to be sent and the start & stop bits (start bit (1<<0) is already 0)
  local_tx_shift_reg = (character << 1) | (1 << 9);  //stop bit (1<<9)
  tx_shift_reg = local_tx_shift_reg;
  //start timer0 with a prescaler of 8
  TCCR0B = (1 << CS01);
}

void UART_tx_str(char* string) {
  while (*string) {
    UART_tx(*string++);
    //wait until transmission is finished
    while (tx_shift_reg)
      ;
  }
}

void UART_init() {
  //set TX pin as output
  TX_DDR |= (1 << TX_DDR_PIN);
  TX_PORT |= (1 << TX_PIN);
  //set timer0 to CTC mode
  TCCR0A = (1 << WGM01);
  //enable output compare 0 A interrupt
  TIMSK0 |= (1 << OCF0A);
  //set compare value to 103 to achieve a 9600 baud rate (i.e. 104µs)
  //together with the 8MHz/8=1MHz timer0 clock
  /*NOTE: since the internal 8MHz oscillator is not very accurate, this value can be tuned
     to achieve the desired baud rate, so if it doesn't work with the nominal value (103), try
     increasing or decreasing the value by 1 or 2 */
  OCR0A = 103;
  //enable interrupts
  sei();
}

//timer0 compare A match interrupt
ISR(TIM0_COMPA_vect) {
  uint16_t local_tx_shift_reg = tx_shift_reg;
  //output LSB of the TX shift register at the TX pin
  if (local_tx_shift_reg & 0x01) {
    TX_PORT |= (1 << TX_PIN);
  } else {
    TX_PORT &= ~(1 << TX_PIN);
  }
  //shift the TX shift register one bit to the right
  local_tx_shift_reg >>= 1;
  tx_shift_reg = local_tx_shift_reg;
  //if the stop bit has been sent, the shift register will be 0
  //and the transmission is completed, so we can stop & reset timer0
  if (!local_tx_shift_reg) {
    TCCR0B = 0;
    TCNT0 = 0;
  }
}