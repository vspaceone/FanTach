#ifndef TSS_h
#define TSS_h
#include <inttypes.h>
#include "Stream.h"

/* Where should this work?
 * ATtiny x4:   Yes - RX on PB0, TX on PB1
 */

  #define SOFTSERIAL_DDR ANALOG_COMP_DDR
  #define SOFTSERIAL_PORT ANALOG_COMP_PORT
  #define SOFTSERIAL_PIN ANALOG_COMP_PIN
  
  
  #define SOFTSERIAL_TXBIT PB1

  #undef SOFT_TX_ONLY
  #define SOFTSERIAL_RXBIT PB0
  #define SOFTSERIAL_vect PCINT1_vect
  
  #if !defined(ACSR) && defined(ACSRA)
    #define ACSR ACSRA
  #endif
  
  #ifndef SOFT_TX_ONLY
    #if (RAMEND < 250)
      #define SERIAL_BUFFER_SIZE 8
    #elif (RAMEND < 500)
      #define SERIAL_BUFFER_SIZE 16
    #elif (RAMEND < 1000)
      #define SERIAL_BUFFER_SIZE 32
    #else
      /* never true for supported parts */
      #define SERIAL_BUFFER_SIZE 128
    #endif
	
    struct tss_soft_ring_buffer
    {
      volatile unsigned char buffer[SERIAL_BUFFER_SIZE];
      volatile uint8_t head; // Making these uint8_t's saves FIFTY BYTES of flash! ISR code-size amplification in action...
      volatile uint8_t tail;
    };
  #endif
  
  extern "C"{
    void uartDelayTSS() __attribute__ ((naked, used)); //used attribute needed to prevent LTO from throwing it out.
    #ifndef SOFT_TX_ONLY
      // manually inlined because the compiler refused to do it.
      //uint8_t getch();
      //void store_char(unsigned char c, tss_soft_ring_buffer *buffer);
    #endif
  }
  class TSS : public Stream
  {
    public: //should be private but needed by extern "C" {} functions.
      uint8_t _txmask;
    #if !defined(SOFT_TX_ONLY)
      tss_soft_ring_buffer *_TSS_rx_buffer;
    #endif
    uint8_t _delayCount;
    public:
      #if !defined(SOFT_TX_ONLY)
        TSS(tss_soft_ring_buffer *TSS_rx_buffer);
      #else
        TSS();
      #endif
      void begin(long);
      void setTxBit(uint8_t);
      void end();          // Basic printHex() forms for 8, 16, and 32-bit values
      void                printHex(const     uint8_t              b);
      void                printHex(const    uint16_t  w, bool s = 0);
      void                printHex(const    uint32_t  l, bool s = 0);
      // printHex(signed) and printHexln() - trivial implementation;
      void                printHex(const      int8_t  b)              {printHex((uint8_t )   b);           }
      void                printHex(const        char  b)              {printHex((uint8_t )   b);           }
      void              printHexln(const      int8_t  b)              {printHex((uint8_t )   b); println();}
      void              printHexln(const        char  b)              {printHex((uint8_t )   b); println();}
      void              printHexln(const     uint8_t  b)              {printHex(             b); println();}
      void              printHexln(const    uint16_t  w, bool s = 0)  {printHex(          w, s); println();}
      void              printHexln(const    uint32_t  l, bool s = 0)  {printHex(          l, s); println();}
      void              printHexln(const     int16_t  w, bool s = 0)  {printHex((uint16_t)w, s); println();}
      void              printHexln(const     int32_t  l, bool s = 0)  {printHex((uint16_t)l, s); println();}
      // The pointer-versions for mass printing uint8_t and uint16_t arrays.
      uint8_t *           printHex(          uint8_t* p, uint8_t len, char sep = 0            );
      uint16_t *          printHex(         uint16_t* p, uint8_t len, char sep = 0, bool s = 0);
      virtual int available(void);
      virtual int peek(void);
      virtual int read(void);
      virtual void flush(void);
      virtual size_t write(uint8_t);
      using Print::write; // pull in write(str) and write(buf, size) from Print
      operator bool();
    private:
      uint8_t _begun = 0;
  };

  extern TSS TinySer;

  //extern void putch(uint8_t);
  #endif
