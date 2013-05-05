/*
 * Author: Maarten van Ingen
 * Description: Energymeter Readout module with integrated webserver
 *
 * Webserver & enc28j60 driver by  Guido Socher http://tuxgraphics.org/electronics/
*/
#include <avr/io.h>
#include <stdlib.h>
#include <string.h>
#include "ip_arp_udp_tcp.h"
#include "enc28j60.h"
#include "timeout.h"
//#include <stdio.h>
#include <avr/eeprom.h>
#include "config.h"

// Array of readings: we only register the last 120 minutes of rotations passed. In one minute we can register no more than 255 rotations, which should more than enough
volatile uint8_t readings[120];

// --------------- Begin TCRT stuff --------------------

#include "usart.h"
#include <avr/interrupt.h>
#include <inttypes.h>



const float whPerPulse = 1000.0f/PULSE_PER_KWH;
volatile uint32_t miliSecondsElapsed = 0;
volatile uint8_t toggle = 0;
volatile uint16_t seenRotations = 0;
volatile uint8_t currentMinute = 0;
volatile uint16_t currentUsage = 0;
volatile uint32_t miliSecondsElapsedSinceLastPulse = 0;


void ADC_init(void)             // Initialization of ADC
{
        ADMUX=(1<<REFS0);       // AVcc with external capacitor at AREF
        SFIOR=0x00;             // Actually default value, but makes sure ADC is in free running mode
        ADCSRA=(1<<ADEN)|(1<<ADATE)|(1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);
                                                // Enable ADC and set Prescaler division factor as 128 and enables autotrigger (ADATE)
        ADCSRA |= (1<<ADSC);
}

void initTimer ()
{
   TCCR1B |= (1 << WGM12); // Configure timer 1 for CTC mode
   TIMSK |= (1 << OCIE1A); // Enable CTC interrupt
   sei(); //  Enable global interrupts
   //OCR1A   = 16000; // Set CTC compare value to 1000Hz at 16MHz AVR clock, with a prescaler of 1
   //OCR1A   = 16000 * TIME_BETWEEN_POLLS; // Set CTC compare value to 1000Hz/TIME_BETWEEN_POLLS at 16MHz AVR clock, with a prescaler of 1
   OCR1A   = 16000; // Set CTC compare value to 1000Hz/TIME_BETWEEN_POLLS at 16MHz AVR clock, with a prescaler of 1
   TCCR1B |= (1 << CS10); // Start timer at Fcpu/1
}


ISR(TIMER1_COMPA_vect)
{
  //miliSecondsElapsed += TIME_BETWEEN_POLLS;
  miliSecondsElapsed++;
  miliSecondsElapsedSinceLastPulse++;
  // If the the reflection is higher, then de ADC readout is lower. So the black marking has a higher readout than the disc itself
  if ( ADC < ADCHIGH ) // We have a reflecting disc in front of the sensor
  {
    toggle = 0;
  }
  else  // We have a lesser reflecting part in front of the sensor (e.g. the black stripe) 
  {
    if ( toggle == 0 )
    {
      seenRotations++;
      // Calculate the current usage based on the time between pulses
      // 375 rotations (my meter) is one kWh so 1 rotation is 2.6667 Wh
      // Next calculation will be in watts, rounded down.
      currentUsage = (1000/PULSE_PER_KWH)*(3600000/miliSecondsElapsedSinceLastPulse);
      miliSecondsElapsedSinceLastPulse = 0;
      toggle = 1;
    }
  }
  //if ( miliSecondsElapsed >= 60000 )
  if ( miliSecondsElapsed >= 6000 )
  {
    // A minute has elapsed and thus we need to register the current amount of rotations seen in the last minute
    readings[currentMinute] = seenRotations;
    currentMinute++;
    if (currentMinute > 119 )
    {
      currentMinute = 0;
    }
    miliSecondsElapsed = 0;
    seenRotations = 0;
  }
}

// --------------- End of TCRT stuff --------------------



static uint8_t buf[BUFFER_SIZE+1];

uint16_t http200ok(void)
{
  return(fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nPragma: no-cache\r\n\r\n")));
}

uint16_t printIndex ( uint8_t *buf )
{
  uint16_t plen;
  plen=http200ok();
  plen=fill_tcp_data_p ( buf, plen, PSTR("<!DOCTYPE html>\n<html>\n<body>\n<a href=\"wlcm.htm\" target=\"data\">Welcome</a><br>\n<a href=\"now.htm\" target=\"data\">Current</a>\n<br>\n<a href=\"min0.htm\" target=\"data\">Minutes</a>\n<br>\n<a href=\"min1.htm\" target=\"data\">Minutes</a>\n<iframe src=\"wlcm.htm\" name=\"data\" style=\"border-color: #0000FF; border: 1; position:absolute; top:10; left:150px; right:0; bottom:10; width:600px; height:95%\"></iframe>\n</body>\n</html>") );
  return( plen );
}

uint16_t printNowHTML ( uint8_t *buf )
{
  char buffer[10];
  uint16_t plen;
  uint8_t i;
  plen=http200ok();
  plen=fill_tcp_data_p ( buf, plen, PSTR("<!DOCTYPE html>\n<html>\n<body>\n<pre>\n") );
  itoa (currentUsage, buffer, 10);
  plen=fill_tcp_data(buf, plen, buffer);
  plen=fill_tcp_data_p ( buf, plen, PSTR("</pre>\n</body>\n</html>") );
  return( plen );
}

uint16_t printMinHTML ( uint8_t *buf, char whichSection )
{
  char buffer[10];
  uint16_t plen;
  uint8_t i;
  plen=http200ok();
  plen=fill_tcp_data_p ( buf, plen, PSTR("<!DOCTYPE html>\n<html>\n<body>\n<pre>\n") );
  for ( i = 60*whichSection ; i < 60 * whichSection + 60 ; i++ )
  {
    plen=fill_tcp_data_p(buf,plen,PSTR("\n#"));
    itoa (i, buffer, 10);
    plen=fill_tcp_data(buf, plen, buffer);
    plen=fill_tcp_data_p(buf,plen,PSTR(":"));
    itoa (readings[i], buffer, 10);
    plen=fill_tcp_data(buf, plen, buffer);
  }
  plen=fill_tcp_data_p ( buf, plen, PSTR("</pre>\n</body>\n</html>") );
  return( plen );
}

uint16_t printWelcomeHTML ( uint8_t *buf )
{
  uint16_t plen;
  plen=http200ok();
  plen=fill_tcp_data_p ( buf, plen, PSTR("<!DOCTYPE html>\n <html>\n <body>\n <p>Welcome to EnergyMeter v0.1</p>\n <p>Select what data to see on the left</p>\n <p>(c)2013 Maarten van Ingen\n </body>\n </html>\n" ) );
  return( plen );
}

int main(void)
{
  uint16_t dat_p;
        
  ADC_init();
  _delay_ms ( 100 );
  //DDRB |= (1<<PB0);

  initTimer();



  //initialize the hardware driver for the enc28j60
  enc28j60Init(mymac);
  enc28j60clkout(2); // change clkout from 6.25MHz to 12.5MHz
  _delay_loop_1(0); // 60us
  enc28j60PhyWrite(PHLCON,0x476);
        
  //init the ethernet/ip layer:
  init_udp_or_www_server(mymac,myip);
  www_server_port(MYWWWPORT);

  while(1)
  {
    // read packet, handle ping and wait for a tcp packet:
    dat_p=packetloop_arp_icmp_tcp(buf,enc28j60PacketReceive(BUFFER_SIZE, buf));

    // dat_p will be unequal to zero if there is a valid  http get
    if(dat_p==0)
    {
	// no http request
	continue;
    }
    // tcp port 80 begin
    if (strncmp("GET ",(char *)&(buf[dat_p]),4)!=0)
    {
      // head, post and other methods:
      dat_p=http200ok();
      dat_p=fill_tcp_data_p(buf,dat_p,PSTR("<h1>200 OK</h1>"));
      www_server_reply(buf,dat_p);
      continue;
    }
    // just one web page in the "root directory" of the web server
    if (strncmp("/ ",(char *)&(buf[dat_p+4]),2)==0)
    {
      //dat_p=print_webpage(buf);
      dat_p=printIndex(buf);
      www_server_reply(buf,dat_p);
    }
    else if (strncmp("/now.htm", (char *)&(buf[dat_p+4]),8) == 0 )
    {
      dat_p = printNowHTML ( buf );
      www_server_reply( buf, dat_p );
    }
    else if (strncmp("/min0.htm", (char *)&(buf[dat_p+4]),9) == 0 )
    {
      dat_p = printMinHTML ( buf, 0 );
      www_server_reply( buf, dat_p );
    }
    else if (strncmp("/min1.htm", (char *)&(buf[dat_p+4]),9) == 0 )
    {
      dat_p = printMinHTML ( buf, 1 );
      www_server_reply( buf, dat_p );
    }
    else if (strncmp("/wlcm.htm", (char *)&(buf[dat_p+4]),9) == 0 )
    {
      dat_p = printWelcomeHTML ( buf );
      www_server_reply( buf, dat_p );
    }
    else
    {
      dat_p=fill_tcp_data_p(buf,0,PSTR("HTTP/1.0 401 Unauthorized\r\nContent-Type: text/html\r\n\r\n<h1>401 Unauthorized</h1>"));
      www_server_reply(buf,dat_p);
    }
  }
  return (0);
}
