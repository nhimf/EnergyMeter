#ifndef USART_H
#define USART_H

#include <avr/io.h>
#include <string.h>

// Define baud rate
#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

void USART_Init(void);
void USART_SendByte(uint8_t u8Data);
void USART_SendString ( char * string );
uint8_t USART_ReceiveByte();

#endif
