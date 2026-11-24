/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Alexis Arakis (Bungle Society) */
/* midi_trs_out.c on the host: USB-MIDI packets out as plain MIDI bytes, a full ring drops a message whole.
 *   cc -Wall -I. -o build/host/trs_out_test tests/trs_out_test.c && build/host/trs_out_test */
#include <stdint.h>
#include <stdio.h>
#define RING_PUBLISH() do{}while(0)
static uint8_t sent[1024]; static int ns, busy;
static void fm1_uart1_tx_init(void){}
static uint32_t fm1_uart1_tx_ready(void){ return !busy; }
static void fm1_uart1_tx_put(uint32_t b){ sent[ns++]=(uint8_t)b; }
#include "firmware/src/midi_trs_out.c"
static uint32_t pk(int cin,int a,int b,int c){return cin|(a<<8)|(b<<16)|(c<<24);}
int main(void){ int f=0;
 trs_out_init();
 trs_out_event(pk(9,0x90,60,100)); trs_out_event(pk(8,0x80,60,0)); trs_out_event(pk(0xB,0xB0,1,64));
 trs_out_event(pk(0xF,0xF8,0,0)); trs_out_event(pk(0xC,0xC0,5,0)); trs_out_event(pk(0,0,0,0));
 for(int i=0;i<50;i++) trs_out_poll();
 uint8_t want[]={0x90,60,100,0x80,60,0,0xB0,1,64,0xF8,0xC0,5};
 if(ns!=sizeof want) f++;
 for(int i=0;i<ns&&i<(int)sizeof want;i++) if(sent[i]!=want[i]) f++;
 /* full ring: drops the message whole */
 ns=0; busy=1; for(int i=0;i<100;i++) trs_out_event(pk(9,0x90,i&127,1)); busy=0;
 for(int i=0;i<1000;i++) trs_out_poll();
 if(ns%3||ns>256) f++;
 for(int i=0;i<ns;i+=3) if(sent[i]!=0x90) f++;
 printf("trs: %d bytes after the full ring, %s\n", ns, f?"FAIL":"ok"); return f!=0; }
