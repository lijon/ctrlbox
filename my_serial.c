/*
  wiring_serial.c - serial functions.
  Part of Arduino - http://www.arduino.cc/

  Copyright (c) 2005-2006 David A. Mellis

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA

  $Id: wiring.c 248 2007-02-03 15:36:30Z mellis $

  Modified on March 1, 2009 by Norbert Pozar
*/

#include "wiring_private.h"

// Define constants and variables for buffering incoming serial data.  We're
// using a ring buffer (I think), in which rx_buffer_head is the index of the
// location to which to write the next incoming character and rx_buffer_tail
// is the index of the location from which to read.

// use only powers of 2 for the buffer size to produce optimal code
// 8, 16, 32, 64, 128 or 256

// input buffer
#define RX_BUFFER_SIZE 128
// output buffer
// set to 0 to disable the output buffer altogether (saves space)
#define TX_BUFFER_SIZE 32

unsigned char rx_buffer[RX_BUFFER_SIZE];

unsigned char rx_buffer_head = 0;
unsigned char rx_buffer_tail = 0;


void beginSerial(long baud)
{

#if defined(__AVR_ATmega8__)
	UBRRH = ((F_CPU / 16 + baud / 2) / baud - 1) >> 8;
	UBRRL = ((F_CPU / 16 + baud / 2) / baud - 1);
	
	// enable rx and tx
	sbi(UCSRB, RXEN);
	sbi(UCSRB, TXEN);
	
	// enable interrupt on complete reception of a byte
	sbi(UCSRB, RXCIE);
#else
	UBRR0H = ((F_CPU / 16 + baud / 2) / baud - 1) >> 8;
	UBRR0L = ((F_CPU / 16 + baud / 2) / baud - 1);
	
	// enable rx and tx
	sbi(UCSR0B, RXEN0);
	sbi(UCSR0B, TXEN0);
	
	// enable interrupt on complete reception of a byte
	sbi(UCSR0B, RXCIE0);
#endif

	// defaults to 8-bit, no parity, 1 stop bit
}

// advanced function, saves ~200 bytes because it doesn't use the long division
// prescaler for the baud rate can be found using the formula
// prescaler = fClock / (16 * baud rate) - 1
// see beginSerial
void beginSerialWithPrescaler(unsigned int prescaler)
{

#if defined(__AVR_ATmega8__)
	UBRRH = prescaler >> 8;
	UBRRL = prescaler;
	
	// enable rx and tx
	sbi(UCSRB, RXEN);
	sbi(UCSRB, TXEN);
	
	// enable interrupt on complete reception of a byte
	sbi(UCSRB, RXCIE);
#else
	UBRR0H = prescaler >> 8;
	UBRR0L = prescaler;
	
	// enable rx and tx
	sbi(UCSR0B, RXEN0);
	sbi(UCSR0B, TXEN0);
	
	// enable interrupt on complete reception of a byte
	sbi(UCSR0B, RXCIE0);
#endif

	// defaults to 8-bit, no parity, 1 stop bit
}

int serialAvailable()
{
	unsigned char i = RX_BUFFER_SIZE + rx_buffer_head - rx_buffer_tail;
	i %= RX_BUFFER_SIZE;

	return i;
}

int serialRead()
{
	// if the head isn't ahead of the tail, we don't have any characters
	if (rx_buffer_head == rx_buffer_tail) {
		return -1;
	} else {
		unsigned char c = rx_buffer[rx_buffer_tail];
		rx_buffer_tail = rx_buffer_tail + 1;
		rx_buffer_tail %= RX_BUFFER_SIZE;
		return c;
	}
}

void serialFlush()
{
	// don't reverse this or there may be problems if the RX interrupt
	// occurs after reading the value of rx_buffer_head but before writing
	// the value to rx_buffer_tail; the previous value of rx_buffer_head
	// may be written to rx_buffer_tail, making it appear as if the buffer
	// were full, not empty.
	rx_buffer_head = rx_buffer_tail;
}

#if defined(__AVR_ATmega8__)
SIGNAL(SIG_UART_RECV)
#else
SIGNAL(USART_RX_vect)
#endif
{
#if defined(__AVR_ATmega8__)
	unsigned char c = UDR;
#else
	unsigned char c = UDR0;
#endif

	unsigned char i = rx_buffer_head + 1;
	i %= RX_BUFFER_SIZE;

	// if we should be storing the received character into the location
	// just before the tail (meaning that the head would advance to the
	// current location of the tail), we're about to overflow the buffer
	// and so we don't write the character or advance the head.
	if (i != rx_buffer_tail) {
		rx_buffer[rx_buffer_head] = c;
		rx_buffer_head = i;
	}
}

// buffered output
#if (TX_BUFFER_SIZE > 0 )

// TX is buffered
// ************************
// tested only for ATmega168
// ************************

unsigned char tx_buffer[TX_BUFFER_SIZE];

unsigned char tx_buffer_head = 0;
volatile unsigned char tx_buffer_tail = 0;


// interrupt called on Data Register Empty
#if defined(__AVR_ATmega8__)
SIGNAL(SIG_UART_DATA)
#else
SIGNAL(USART_UDRE_vect) 
#endif
{
  // temporary tx_buffer_tail 
  // (to optimize for volatile, there are no interrupts inside an interrupt routine)
  unsigned char tail = tx_buffer_tail;

  // get a byte from the buffer	
  unsigned char c = tx_buffer[tail];
  // send the byte
#if defined(__AVR_ATmega8__)
	UDR = c;
#else
	UDR0 = c;
#endif

  // update tail position
  tail ++;
  tail %= TX_BUFFER_SIZE;

  // if the buffer is empty,  disable the interrupt
  if (tail == tx_buffer_head) {
#if defined(__AVR_ATmega8__)
    UCSRB &=  ~(1 << UDRIE);
#else
    UCSR0B &=  ~(1 << UDRIE0);
#endif

  }

  tx_buffer_tail = tail;

}


void serialWrite(unsigned char c) {

#if defined(__AVR_ATmega8__)
 	if ((!(UCSRA & (1 << UDRE)))
#else
	if ((!(UCSR0A & (1 << UDRE0)))
#endif 
  || (tx_buffer_head != tx_buffer_tail)) {
    // maybe checking if buffer is empty is not necessary, 
    // not sure if there can be a state when the data register empty flag is set
    // and read here without the interrupt being executed
    // well, it shouldn't happen, right?

    // data register is not empty, use the buffer
    unsigned char newhead = tx_buffer_head + 1;
    newhead %= TX_BUFFER_SIZE;

    // wait until there's a space in the buffer
    while (newhead == tx_buffer_tail) ;

    tx_buffer[tx_buffer_head] = c;
    tx_buffer_head = newhead;

    // enable the Data Register Empty Interrupt
#if defined(__AVR_ATmega8__)
    UCSRB |=  (1 << UDRIE);
#else
    UCSR0B |=  (1 << UDRIE0);
#endif

  } 
  else {
    // no need to wait
#if defined(__AVR_ATmega8__)
	UDR = c;
#else
	UDR0 = c;
#endif
  }
}

#else

// unbuffered write
void serialWrite(unsigned char c)
{
#if defined(__AVR_ATmega8__)
	while (!(UCSRA & (1 << UDRE)))
		;

	UDR = c;
#else
	while (!(UCSR0A & (1 << UDRE0)))
		;

	UDR0 = c;
#endif
}

#endif

