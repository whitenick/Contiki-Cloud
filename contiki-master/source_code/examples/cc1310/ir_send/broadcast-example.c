/*
 * Copyright (c) 2011, Swedish Institute of Computer Science.
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

#include "contiki.h"

/* HW */
#include "board.h"
#include "board-i2c.c"
#include "dev/leds.h"
#include "dev/button-sensor.h"
#include "ti-lib.h"

/* SW */
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"

#include "simple-udp.h"

#include <stdio.h>
#include <string.h>

#define UDP_PORT 1234

#define SEND_INTERVAL		(1 * CLOCK_SECOND)
#define SEND_TIME		(random_rand() % (SEND_INTERVAL))

/* INFRARED SENSOR I2C */
#define D6T_ADDR 0x0A // Address of OMRON D6T is 0x0A in hex
#define D6T_CMD 0x4C // Standard command is 4C in hex
uint8_t         txBuffer[1];
uint8_t         rxBuffer[35];
int             temps[17];

char buffer[200];

/* RF */
static struct etimer periodic_timer;
static struct etimer send_timer;
uip_ipaddr_t addr;

static struct simple_udp_connection broadcast_connection;

#define uip_ipaddr_to_eight(a) (a)->u16[0],(a)->u16[1],(a)->u16[2],(a)->u16[3],(a)->u16[4],(a)->u16[5],(a)->u16[6],(a)->u16[7]
#define uip_ipaddr_to_bytes(a) (a)->u8[0],(a)->u8[1],(a)->u8[2],(a)->u8[3],(a)->u8[4],(a)->u8[5],(a)->u8[6],(a)->u8[7],(a)->u8[8],(a)->u8[9],(a)->u8[10],(a)->u8[11],(a)->u8[12],(a)->u8[13],(a)->u8[14],(a)->u8[15]

/*---------------------------------------------------------------------------*/
PROCESS(broadcast_example_process, "UDP broadcast example process");
AUTOSTART_PROCESSES(&broadcast_example_process);
/*---------------------------------------------------------------------------*/
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
	/*
  printf("Data received on port %d from port %d with length %d\n",
         receiver_port, sender_port, datalen);
*/
//  printf("Received from %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x\n",uip_ipaddr_to_eight(sender_addr));
  printf("Received from %02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x\n",uip_ipaddr_to_bytes(sender_addr));
  printf("%.*s\n",datalen,data);
 
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_example_process, ev, data)
{
  int i;
  PROCESS_BEGIN();

  /* set up udp */
  simple_udp_register(&broadcast_connection, UDP_PORT, NULL, UDP_PORT, receiver);
  uip_create_linklocal_allnodes_mcast(&addr);

  /* init i2c */
  board_i2c_select(BOARD_I2C_INTERFACE_0, D6T_ADDR);
  txBuffer[0] = D6T_CMD;

  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1){
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);
    board_i2c_wakeup();
    board_i2c_write_read(&txBuffer[0],1,&rxBuffer[0],35);
    for(i=0; i<17; i++){
      temps[i] = rxBuffer[i*2]+rxBuffer[i*2+1]*256;
    }
    /* print to uart */
    printf("%3d\n",temps[0]);
    for(i=0; i<4; i++){
      printf("%3d  %3d  %3d  %3d\n",temps[i*4+1],temps[i*4+2],temps[i*4+3],temps[i*4+4]);
    }
    printf("\n");

    /* print to rf */
    for(i=0; i<17; i++){
    	sprintf(&buffer[i*4],"%3d,",temps[i]);
    }
    buffer[69] = 0;
    simple_udp_sendto(&broadcast_connection, buffer, 68, &addr);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
