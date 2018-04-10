/*
 * Copyright (c) 2006, Swedish Institute of Computer Science.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file is part of the Contiki operating system.
 *
 */

/**
 * \file
 *         A very simple Contiki application showing how Contiki programs look
 * \author
 *         Adam Dunkels <adam@sics.se>
 */

#include "contiki.h"

#include <stdio.h> /* For printf() */

#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "board.h"
#include "ti-lib.h"
#include "board-i2c.c"

#include "sys/ctimer.h"
#include "sys/etimer.h"

#define BUTTON_LEFT  &button_left_sensor
#define BUTTON_RIGHT &button_right_sensor
#define SEND_INTERVAL (CLOCK_SECOND)

#define D6T_ADDR 0x0A // Address of OMRON D6T is 0x0A in hex
#define D6T_CMD 0x4C // Standard command is 4C in hex
uint8_t         txBuffer[1];
uint8_t         rxBuffer[35];
int             temps[17];

/*---------------------------------------------------------------------------*/
PROCESS(hello_world_process, "Hello world process");
AUTOSTART_PROCESSES(&hello_world_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(hello_world_process, ev, data)
{
  static struct etimer periodic_timer;
  int i;

  PROCESS_BEGIN();
  
  printf("Hello, world\n");

  board_i2c_select(BOARD_I2C_INTERFACE_0, D6T_ADDR);
  txBuffer[0] = D6T_CMD;

  printf("I2C init complete\n");

  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1){
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);
    board_i2c_wakeup();
    board_i2c_write_read(&txBuffer[0],1,&rxBuffer[0],35);
    //for(i=0; i<35; i++){
    //  printf("%3d ",rxBuffer[i]);
    //}
    for(i=0; i<17; i++){
      temps[i] = rxBuffer[i*2]+rxBuffer[i*2+1]*256;
    }
    printf("%3d\n",temps[0]);
    for(i=0; i<4; i++){
      printf("%3d  %3d  %3d  %3d\n",temps[i*4+1],temps[i*4+2],temps[i*4+3],temps[i*4+4]);
    }
    printf("\n");
  }
  printf("NOT REACHED\n");
  PROCESS_END();
}

/*---------------------------------------------------------------------------*/
