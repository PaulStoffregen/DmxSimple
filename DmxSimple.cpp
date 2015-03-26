/**
 * DmxSimple - A simple interface to DMX.
 *
 * Copyright (c) 2008-2009 Peter Knight, Tinker.it! All rights reserved.
 */
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "pins_arduino.h"

#include "wiring.h"
#include "DmxSimple.h"

/** dmxBuffer contains a software copy of all the DMX channels.
  */
volatile uint8_t dmxBuffer[DMX_SIZE];
static uint16_t dmxMax = 16; /* Default to sending the first 16 channels */
static uint8_t dmxStarted = 0;
static uint16_t dmxState = 0;

static volatile uint8_t *dmxPort;
static uint8_t dmxBit = 0;
static uint8_t dmxPin = 3; // Defaults to output on pin 3 to support Tinker.it! DMX shield

void dmxBegin();
void dmxEnd();
void dmxSendByte(volatile uint8_t);
void dmxWrite(int,uint8_t);
void dmxMaxChannel(int);

/* TIMER2 has a different register mapping on the ATmega8.
 * The modern chips (168, 328P, 1280) use identical mappings.
 */
#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega168P__) || defined(__AVR_ATmega328P__) || defined(__AVR_ATmega1280__)
#define TIMER2_INTERRUPT_ENABLE() TIMSK2 |= _BV(TOIE2)
#define TIMER2_INTERRUPT_DISABLE() TIMSK2 &= ~_BV(TOIE2)
#elif defined(__AVR_ATmega8__)
#define TIMER2_INTERRUPT_ENABLE() TIMSK |= _BV(TOIE2)
#define TIMER2_INTERRUPT_DISABLE() TIMSK &= ~_BV(TOIE2)
#else
#define TIMER2_INTERRUPT_ENABLE()
#define TIMER2_INTERRUPT_DISABLE()
/* Produce an appropriate message to aid error reporting on nonstandard
 * platforms such as Teensy.
 */
#warning "DmxSimple does not support this CPU"
#endif


/** Initialise the DMX engine
 */
void dmxBegin()
{
  dmxStarted = 1;

  // Set up port pointers for interrupt routine
  dmxPort = portOutputRegister(digitalPinToPort(dmxPin));
  dmxBit = digitalPinToBitMask(dmxPin);

  // Set DMX pin to output
  pinMode(dmxPin,OUTPUT);

  // Initialise DMX frame interrupt
  //
  // Presume Arduino has already set Timer2 to 64 prescaler,
  // Phase correct PWM mode
  // So the overflow triggers every 64*510 clock cycles
  // Which is 510 DMX bit periods at 16MHz,
  //          255 DMX bit periods at 8MHz,
  //          637 DMX bit periods at 20MHz
  TIMER2_INTERRUPT_ENABLE();
}

/** Stop the DMX engine
 * Turns off the DMX interrupt routine
 */
void dmxEnd()
{
  TIMER2_INTERRUPT_DISABLE();
  dmxStarted = 0;
  dmxMax = 0;
}

/** Transmit a complete DMX byte
 * We have no serial port for DMX, so everything is timed using an exact
 * number of instruction cycles.
 *
 * Really suggest you don't touch this function.
 */
void dmxSendByte(volatile uint8_t value)
{
  uint8_t bitCount, delCount;
  __asm__ volatile (
    "cli\n"
    "ld __tmp_reg__,%a[dmxPort]\n"
    "and __tmp_reg__,%[outMask]\n"
    "st %a[dmxPort],__tmp_reg__\n"
    "ldi %[bitCount],11\n" // 11 bit intervals per transmitted byte
    "rjmp bitLoop%=\n"     // Delay 2 clock cycles. 
  "bitLoop%=:\n"\
    "ldi %[delCount],%[delCountVal]\n"
  "delLoop%=:\n"
    "nop\n"
    "dec %[delCount]\n"
    "brne delLoop%=\n"
    "ld __tmp_reg__,%a[dmxPort]\n"
    "and __tmp_reg__,%[outMask]\n"
    "sec\n"
    "ror %[value]\n"
    "brcc sendzero%=\n"
    "or __tmp_reg__,%[outBit]\n"
  "sendzero%=:\n"
    "st %a[dmxPort],__tmp_reg__\n"
    "dec %[bitCount]\n"
    "brne bitLoop%=\n"
    "sei\n"
    :
      [bitCount] "=&d" (bitCount),
      [delCount] "=&d" (delCount)
    :
      [dmxPort] "e" (dmxPort),
      [outMask] "r" (~dmxBit),
      [outBit] "r" (dmxBit),
      [delCountVal] "M" (F_CPU/1000000-3),
      [value] "r" (value)
  );
}

/** DmxSimple interrupt routine
 * Transmit a chunk of DMX signal every timer overflow event.
 * 
 * The full DMX transmission takes too long, but some aspects of DMX timing
 * are flexible. This routine chunks the DMX signal, only sending as much as
 * it's time budget will allow.
 *
 * This interrupt routine runs with interrupts enabled most of the time.
 * With extremely heavy interrupt loads, it could conceivably interrupt its
 * own routine, so the TIMER2 interrupt is disabled for the duration of
 * the service routine.
 */
ISR(TIMER2_OVF_vect,ISR_NOBLOCK) {

  // Prevent this interrupt running recursively
  TIMER2_INTERRUPT_DISABLE();

  uint16_t bitsLeft = F_CPU / 31372; // DMX Bit periods per timer tick
  bitsLeft >>=2; // 25% CPU usage
  while (1) {
    if (dmxState == 0) {
      // Next thing to send is reset pulse and start code
      // which takes 35 bit periods
      uint8_t i;
      if (bitsLeft < 35) break;
      bitsLeft-=35;
      *dmxPort &= ~dmxBit;
      for (i=0; i<11; i++) _delay_us(8);
      *dmxPort |= dmxBit;
      _delay_us(8);
      dmxSendByte(0);
    } else {
      // Now send a channel which takes 11 bit periods
      if (bitsLeft < 11) break;
      bitsLeft-=11;
      dmxSendByte(dmxBuffer[dmxState-1]);
    }
    // Successfully completed that stage - move state machine forward
    dmxState++;
    if (dmxState > dmxMax) {
      dmxState = 0; // Send next frame
      break;
    }
  }
  
  // Enable interrupts for the next transmission chunk
  TIMER2_INTERRUPT_ENABLE();
}

void dmxWrite(int channel, uint8_t value) {
  if (!dmxStarted) dmxBegin();
  if ((channel > 0) && (channel <= DMX_SIZE)) {
    if (value<0) value=0;
    if (value>255) value=255;
    dmxMax = max((unsigned)channel, dmxMax);
    dmxBuffer[channel-1] = value;
  }
}

void dmxMaxChannel(int channel) {
  if (channel <=0) {
    // End DMX transmission
    dmxEnd();
    dmxMax = 0;
  } else {
    dmxMax = min(channel, DMX_SIZE);
    if (!dmxStarted) dmxBegin();
  }
}


/* C++ wrapper */


/** Set output pin
 * @param pin Output digital pin to use
 */
void DmxSimpleClass::usePin(uint8_t pin) {
  dmxPin = pin;
  if (dmxStarted && (pin != dmxPin)) {
    dmxEnd();
    dmxBegin();
  }
}

/** Set DMX maximum channel
 * @param channel The highest DMX channel to use
 */
void DmxSimpleClass::maxChannel(int channel) {
  dmxMaxChannel(channel);
}

/** Write to a DMX channel
 * @param address DMX address in the range 1 - 512
 */
void DmxSimpleClass::write(int address, uint8_t value)
{
	dmxWrite(address, value);
}
DmxSimpleClass DmxSimple;
