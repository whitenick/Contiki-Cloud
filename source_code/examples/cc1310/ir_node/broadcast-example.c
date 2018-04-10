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
#include "st7735.h"

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

int str3toint(char *a){
  int out=0;
  if(a[0]!=' ')
    out += (a[0]-'0')*100;
  if(a[1]!=' ')
    out += (a[1]-'0')*10;
  if(a[2]!=' ')
    out += a[2]-'0';
  return out;
}

int lastVals[17];
int controlVals[17];
int motionTime = 100;
int controlTime = 100;
int httpTime = 100;
bool empty = true;
bool prevEmpty = true;
bool init = true;
#define MOTION_TIMEOUT  10
#define CONTROL_TIMEOUT 20
#define HTTP_TIMEOUT 30

int sum16(int *a){
  int sum = 0;
  int i;
  for(i=0; i<16; i++){
    sum += a[i];
  }
  return sum;
}

int min16(int *a){
  int min = 1000000;
  int i;
  for(i=0; i<16; i++){
    if(min>a[i]){
      min = a[i];
    }
  }
  return min;
}

int max16(int *a){
  int max = -1000000;
  int i;
  for(i=0; i<16; i++){
    if(max<a[i]){
      max = a[i];
    }
  }
  return max; 
}

int var16(int *a){
  int sum = sum16(a);
  int var = 0;
  int i, x;
  for(i=0; i<16; i++){
    x = a[i]*16 - sum;
    x = x * x;
    var += x;
  }
  return var;
}

int mse16(int *a, int *b){
  int mse = 0;
  int i, x;
  for(i=0; i<16; i++){
    x = a[i] - b[i];
    x = x * x;
    mse += x;
  }
  return mse;
}

bool detect(int *vals){
  int mse, mse_c;
  prevEmpty = empty;
  empty = true;
  int i;

  printf("*** detect logic ***\n");

  if(init){
    for(i=1; i<17; i++){
      lastVals[i] = vals[i];
    }
    for(i=1; i<17; i++){
      controlVals[i] = vals[i];
    }
    init = false;
  }

  mse = mse16(&vals[1],&lastVals[1]);
  printf("mean square error = %d\n",mse);
  printf("motion time = %d\n",motionTime);
  
  if(mse > 1000){
    motionTime = 0;
  }
  if(motionTime < MOTION_TIMEOUT){
    printf("HUMAN DETECTED! HUMAN DETECTED! HUMAN DETECTED!\n");
    empty = false;
  }

  // update control when everything is stable
  mse_c = mse16(&vals[1],&controlVals[1]);
  printf("mean-square-error control = %d\n",mse_c);
  printf("control time = %d\n",controlTime);

  if(empty && mse_c < 1000){
    printf("empty room: update control vals\n");
    for(i=1; i<17; i++){
      controlVals[i] = lastVals[i];
    }
  }

  if(mse_c > 1000){
    printf("human presense detected\n");
    empty = false;
  }

  // update last vals
  for(i=1; i<17; i++){
    lastVals[i] = vals[i];
  }

  motionTime++;
  controlTime++;
  
  return empty;
}

void stats(int *vals){
  int min, max;
  int sum, mean, var;
  min = min16(&vals[1]);
  max = max16(&vals[1]);
  sum = sum16(&vals[1]);
  mean = sum/16;
  var = var16(&vals[1]);

  printf("*** statistics ***\n");
  printf("min = %d\nmax = %d\n",min,max);
  printf("max - min = %d\n",max-min);
  printf("mean = %d\nvariance = %d\n",mean,var);  
}


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
  // convert ip address to some id
  // print id

  // print true / false
  printf("%.*s\n",datalen,data);

/*
  int i;
  int vals[17];
  for(i=0; i<17; i++){
    vals[i] = str3toint(&data[i*4]);
  }
  printf("%3d\n",vals[0]);
  for(i=0; i<4; i++){
    printf("%3d  %3d  %3d  %3d\n",vals[i*4+1],vals[i*4+2],vals[i*4+3],vals[i*4+4]);
  }
  printf("\n");

  stats(vals);
  */
}


bool emptyRoom = false;
bool lastEmpty = false;

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_example_process, ev, data)
{
  int i;
  PROCESS_BEGIN();

  /* set up udp */
  simple_udp_register(&broadcast_connection, UDP_PORT, NULL, UDP_PORT, receiver);
  uip_create_linklocal_allnodes_mcast(&addr);

  /* init st7735 */
  //ST7735_InitR(INITR_REDTAB);
  //ST7735_FillScreen(0);

  /* init i2c */
  board_i2c_select(BOARD_I2C_INTERFACE_0, D6T_ADDR);
  txBuffer[0] = D6T_CMD;

  etimer_set(&periodic_timer, SEND_INTERVAL);
  while(1){
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);

    // i2c poll
    
    board_i2c_wakeup();
    board_i2c_write_read(&txBuffer[0],1,&rxBuffer[0],35);
    for(i=0; i<17; i++){
      temps[i] = rxBuffer[i*2]+rxBuffer[i*2+1]*256;
    }
    

    // print to uart
    
    printf("%3d\n",temps[0]);
    for(i=0; i<4; i++){
      printf("%3d  %3d  %3d  %3d\n",temps[i*4+1],temps[i*4+2],temps[i*4+3],temps[i*4+4]);
    }
    printf("\n");
    stats(temps);
    

    /* print to rf */
    /*
    for(i=0; i<17; i++){
    	sprintf(&buffer[i*4],"%3d,",temps[i]);
    }
    buffer[69] = 0;
    simple_udp_sendto(&broadcast_connection, buffer, 68, &addr);
    */

    lastEmpty = emptyRoom;
    emptyRoom = detect(temps);
    httpTime++;

    if(emptyRoom){
      simple_udp_sendto(&broadcast_connection, "true", 4, &addr);
    } else {
      simple_udp_sendto(&broadcast_connection, "false", 5, &addr);
    }
    /*
    if(emptyRoom != lastEmpty){
      if(emptyRoom){
        simple_udp_sendto(&broadcast_connection, "true", 4, &addr);
        //sprintf(buffer,"true\n");
      } else {
        simple_udp_sendto(&broadcast_connection, "false", 5, &addr);
        //sprintf(buffer,"false\n");
      }
      //httpTime = 0;
    }
    */
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
