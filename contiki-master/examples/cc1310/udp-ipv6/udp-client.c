/*
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
#include "contiki-lib.h"
#include "contiki-net.h"
#include "net/ip/resolv.h"

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

#include <string.h>
#include <stdbool.h>

#define DEBUG DEBUG_PRINT
#include "net/ip/uip-debug.h"

#define SEND_INTERVAL		4 * CLOCK_SECOND
#define MAX_PAYLOAD_LEN		40

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

static struct uip_udp_conn *client_conn;
/*---------------------------------------------------------------------------*/
PROCESS(udp_client_process, "UDP client process");
AUTOSTART_PROCESSES(&resolv_process,&udp_client_process);
/*---------------------------------------------------------------------------*/
static void
tcpip_handler(void)
{
  char *str;

  if(uip_newdata()) {
    str = uip_appdata;
    str[uip_datalen()] = '\0';
    printf("Response from the server: '%s'\n", str);
  }
}

static void
print_local_addresses(void)
{
  int i;
  uint8_t state;

  PRINTF("Client IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      PRINT6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
      PRINTF("\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
#if UIP_CONF_ROUTER
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;

  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);
}
#endif /* UIP_CONF_ROUTER */
/*---------------------------------------------------------------------------*/
static resolv_status_t
set_connection_address(uip_ipaddr_t *ipaddr)
{
#ifndef UDP_CONNECTION_ADDR
#if RESOLV_CONF_SUPPORTS_MDNS
#define UDP_CONNECTION_ADDR       contiki-udp-server.local
#elif UIP_CONF_ROUTER
#define UDP_CONNECTION_ADDR       fd00:0:0:0:0212:7404:0004:0404
#else
#define UDP_CONNECTION_ADDR       fe80:0:0:0:6466:6666:6666:6666
#endif
#endif /* !UDP_CONNECTION_ADDR */

#define _QUOTEME(x) #x
#define QUOTEME(x) _QUOTEME(x)

  resolv_status_t status = RESOLV_STATUS_ERROR;

  if(uiplib_ipaddrconv(QUOTEME(UDP_CONNECTION_ADDR), ipaddr) == 0) {
    uip_ipaddr_t *resolved_addr = NULL;
    status = resolv_lookup(QUOTEME(UDP_CONNECTION_ADDR),&resolved_addr);
    if(status == RESOLV_STATUS_UNCACHED || status == RESOLV_STATUS_EXPIRED) {
      PRINTF("Attempting to look up %s\n",QUOTEME(UDP_CONNECTION_ADDR));
      resolv_query(QUOTEME(UDP_CONNECTION_ADDR));
      status = RESOLV_STATUS_RESOLVING;
    } else if(status == RESOLV_STATUS_CACHED && resolved_addr != NULL) {
      PRINTF("Lookup of \"%s\" succeded!\n",QUOTEME(UDP_CONNECTION_ADDR));
    } else if(status == RESOLV_STATUS_RESOLVING) {
      PRINTF("Still looking up \"%s\"...\n",QUOTEME(UDP_CONNECTION_ADDR));
    } else {
      PRINTF("Lookup of \"%s\" failed. status = %d\n",QUOTEME(UDP_CONNECTION_ADDR),status);
    }
    if(resolved_addr)
      uip_ipaddr_copy(ipaddr, resolved_addr);
  } else {
    status = RESOLV_STATUS_CACHED;
  }

  return status;
}
/*---------------------------------------------------------------------------*/

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

/*---------------------------------------------------------------------------*/
static char buf[MAX_PAYLOAD_LEN];
static void
timeout_handler(void)
{
  static int seq_id;

  printf("Client sending to: ");
  PRINT6ADDR(&client_conn->ripaddr);
  //sprintf(buf, "Hello %d from the client", ++seq_id);
  printf(" (msg: %s)\n", buf);
#if SEND_TOO_LARGE_PACKET_TO_TEST_FRAGMENTATION
  uip_udp_packet_send(client_conn, buf, UIP_APPDATA_SIZE);
#else /* SEND_TOO_LARGE_PACKET_TO_TEST_FRAGMENTATION */
  uip_udp_packet_send(client_conn, buf, strlen(buf));
#endif /* SEND_TOO_LARGE_PACKET_TO_TEST_FRAGMENTATION */
}
/*---------------------------------------------------------------------------*/

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


bool emptyRoom = false;
bool lastEmpty = false;
int mes_no = 0;
char strbuf[64];
int strsize = 0;

char* roomName = "EERC 1.102";
char roomBuff[32];


PROCESS_THREAD(udp_client_process, ev, data)
{
  int i;
  static struct etimer et;
  uip_ipaddr_t ipaddr;

  PROCESS_BEGIN();
  PRINTF("UDP client process started\n");

#if UIP_CONF_ROUTER
  set_global_address();
#endif

  print_local_addresses();
  // set the room addresses
  int end_size = 0;
  end_size = sprintf(roomBuff, "EERC %02x%02x", ipaddr.u8[14], ipaddr.u8[15]);

  static resolv_status_t status = RESOLV_STATUS_UNCACHED;
  while(status != RESOLV_STATUS_CACHED) {
    status = set_connection_address(&ipaddr);

    if(status == RESOLV_STATUS_RESOLVING) {
      PROCESS_WAIT_EVENT_UNTIL(ev == resolv_event_found);
    } else if(status != RESOLV_STATUS_CACHED) {
      PRINTF("Can't get connection address.\n");
      PROCESS_YIELD();
    }
  }

  /* new connection with remote host */
  client_conn = udp_new(&ipaddr, UIP_HTONS(3000), NULL);
  udp_bind(client_conn, UIP_HTONS(3001));

  PRINTF("Created a connection with the server ");
  PRINT6ADDR(&client_conn->ripaddr);
  PRINTF(" local/remote port %u/%u\n",
	UIP_HTONS(client_conn->lport), UIP_HTONS(client_conn->rport));

  // initialize the board
  board_i2c_select(BOARD_I2C_INTERFACE_0, D6T_ADDR);
  txBuffer[0] = D6T_CMD;

  etimer_set(&et, SEND_INTERVAL);
  while(1) {
    PROCESS_YIELD();
    if(etimer_expired(&et)) {

      //printf("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!! %d %d !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n",lastEmpty,emptyRoom);
      // if(emptyRoom != lastEmpty){
        if(emptyRoom){
          printf("room is empty\n");
          strsize = sprintf(buf, "%s+%d+false", roomBuff, mes_no);
          //mes_no++;
          //simple_udp_sendto(&broadcast_connection, strbuf, strsize, &addr);
        } else {
          printf("room is not empty\n");
          strsize = sprintf(buf, "%s+%d+true", roomBuff, mes_no);
          //simple_udp_sendto(&broadcast_connection, strbuf, strsize, &addr);
        }
      //}

      httpTime++;
      mes_no++;
      lastEmpty = emptyRoom;

      timeout_handler();
      etimer_restart(&et);
    } else if(ev == tcpip_event) {
      tcpip_handler();
    }

    // whether a message is sent or received, collect board data regardless
    // and update values
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

    emptyRoom = detect(temps);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
