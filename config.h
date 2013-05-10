/*
 * Name: config.h
 * Author: Maarten van Ingen
 * Copyright: 2013, Maarten van Ingen
 * License: See LICENSE file
*/

#ifndef CONFIG_H
#define CONFIG_H

#define ADCHIGH 840
#define PULSE_PER_KWH 375
#define MIN_MS_COUNT 200
#define TIME_BETWEEN_POLLS 10 // Time between polls in ms. If we poll every 10ms, we poll every 3.6degrees of the disc in case of a rotation of 1 second.

#define MYWWWPORT 80
#define BUFFER_SIZE 700
static uint8_t mymac[6] = {0x54,0x55,0x58,0x10,0x00,0x29};
static uint8_t myip[4] = {192,168,2,99};


#endif
