/*
 * Name: usart.c
 * Author: Maarten van Ingen
 * Copyright: 2013, Maarten van Ingen
 * License: See LICENSE file
*/

#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>

#include "usart.h"

#define ADCHIGH 840
#define PULSE_PER_KWH 375
#define MIN_MS_COUNT 200

const float whPerPulse = 1000.0f/PULSE_PER_KWH;
volatile uint32_t miliSecondsElapsed = 0;


void ADC_init(void)		// Initialization of ADC
{
	ADMUX=(1<<REFS0);	// AVcc with external capacitor at AREF
	SFIOR=0x00;		// Actually default value, but makes sure ADC is in free running mode
	ADCSRA=(1<<ADEN)|(1<<ADATE)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);	
						// Enable ADC and set Prescaler division factor as 128 and enables autotrigger (ADATE)
	ADCSRA |= (1<<ADSC);
}

uint16_t ADC_read( void )
{

	ADMUX &= 0b11100000;			// Select ADC0 (=PA0, pin40), MUX4:0 = 00000, the other bits needs to be the same...

	ADCSRA|=(1<<ADSC);			// start conversion
	while(ADCSRA & (1<<ADSC));	// waiting for ADIF, conversion complete
 
	return (ADC);
}


uint16_t calculateUsage ( uint32_t miliSecondsSinceLastPulse )
{
  // 375 rounds per kwh, 1 rotation = 2.666Wh
  // To get current usage is watts: devide 2.666Wh by time in hours.
  // E.g. 1 rotation in 10 seconds is: 2.666/(10/3600)=957.868
  return whPerPulse / ( (float)miliSecondsSinceLastPulse / 3600000.0f );
}

void initTimer ()
{
   TCCR1B |= (1 << WGM12); // Configure timer 1 for CTC mode 

   TIMSK |= (1 << OCIE1A); // Enable CTC interrupt 

   sei(); //  Enable global interrupts 

   //OCR1A   = 15624; // Set CTC compare value to 1Hz at 1MHz AVR clock, with a prescaler of 64 
   //OCR1A   = 3124; // Set CTC compare value to 10Hz at 8MHz AVR clock, with a prescaler of 256
   OCR1A   = 8000; // Set CTC compare value to 1000Hz at 8MHz AVR clock, with a prescaler of 1

   TCCR1B |= (1 << CS10); // Start timer at Fcpu/1 
   //TCCR1B |= ((1 << CS10) | (1 << CS11)); // Start timer at Fcpu/64 
   //TCCR1B |= (1 << CS12); // Start timer at Fcpu/256
}

ISR(TIMER1_COMPA_vect) 
{ 
  miliSecondsElapsed += 1;
}

int main(void)
{
   uint16_t usage;
   uint32_t secs;
   char buffer[16];
   USART_Init();  // Initialise USART
   _delay_ms ( 100 );
   ADC_init();
   DDRB |= (1<<PB0);
   
   initTimer();


   char toggle = 0;


   usage = calculateUsage ( 10 );
   USART_SendString ( "Proximity detector started\n" );
   
   for(;;){    // Repeat indefinitely
     secs = miliSecondsElapsed;
     {
       if ( ADC > ADCHIGH )
       {
        if ( toggle == 0 )
        {
         miliSecondsElapsed = 0;
         usage = calculateUsage ( secs );
         if ( usage != 65535 )
         {
           PORTB |= (1<<PB0);
           sprintf( buffer, "%u", usage); 
           USART_SendString ( buffer );
           USART_SendString ( "\n" );
         }
         toggle = 1;
        }
       }
       else
       {
          PORTB &= ~(1<<PB0);
	  toggle = 0;
       }
     }
     _delay_ms(1);         
   }

}
