
// AVR C library
//
#include <avr/io.h>
//
// Standard C include files
//
#include <stdbool.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include "inttypes.h"
//
// You MUST include app.h and implement every function declared
//
#include "app.h"
//
// Include the header files for the various required libraries
//
#include "simple_os.h"
#include "button.h"
#include "leds.h"
#include "radio.h"
#include "serial.h"
#include "hw_timer.h"

typedef struct Client {
	uint16_t id;
	uint16_t num;
} Client;


typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	uint16_t num;
} Packet;

typedef struct Req {
	uint16_t dst;
	uint16_t src;
	uint16_t req;
} Req;

unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false;

static timer timer1;
Client cl; //in reality should have an array of these.
Packet newPkt;
Req request;
uint16_t rollingCodeSize = 5;
bool canSend = false;
bool on = false;

void application_start()
{
	cl.id = 0x01; //Initial setup of clients
	cl.num = 15;
	
	leds_init();
	button_init();
	
	radio_init(NODE_ID, false);
	radio_set_power(1);
	radio_start();
	serial_init(9600);
	printf("test");
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 2000);
	timer_start(&timer1);
	
	request.dst = 0xFF;
	request.src = NODE_ID;
	request.req = 1;
}

void application_timer_tick(timer *t)
{
	//every 2 seconds, send out a request for opening.
	if(tx_buffer_inuse == false){
		tx_buffer_inuse = true;
		
		
		memcpy(&tx_buffer, &request, sizeof(Req));
		printf("sending req\r\n");
		radio_send(tx_buffer, sizeof(Req), 0xFFFF); //0xFFFF for broadcast
	}
	//probably don't require ack.
	/*if(canSend){
		if(tx_buffer_inuse == false){
			tx_buffer_inuse = true;
			canSend = false;
			memcpy(&tx_buffer, &acknowledgement, sizeof(Ack));
			//printf("sending ack\r\n");
			radio_send(tx_buffer, sizeof(Ack), acknowledgement.dst);
		}
	}*/
}

void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{
	newPkt.dst = msgdata[0];
	newPkt.src = msgdata[2];
	newPkt.num = msgdata[4];
	
	//printf("Open request from: %d\r\n", newPkt.dst);
	printf("Open request from: %d\r\n", newPkt.src);
	printf("Rolling code value sent: %d\r\n", newPkt.num);
	printf("Actual code: %d\r\n", cl.num);
	printf("Size of window: %d\r\n", rollingCodeSize);
	
	//TODO: fix for wraparound.
	if(newPkt.src == cl.id){ //change this to search array of clients
		if(newPkt.num < cl.num){
			//imposter, the number given is too small. Trying to reuse an already accepted response.
			printf("Number too small, don't open the gate!\r\n\r\n");
			leds_off(LED_GREEN);
			leds_on(LED_RED);
			leds_off(LED_ORANGE);
		}
		else{
			uint16_t temp = newPkt.num - cl.num;
			if(temp <= rollingCodeSize){
				//acceptable packet
				cl.num = newPkt.num + 1; //this is the value we expect the NEXT packet to have.
				printf("Open the gate!\r\n\r\n");
				//flip green light
				if(!on){
					leds_on(LED_GREEN);
					leds_off(LED_RED);
					leds_off(LED_ORANGE);
					on = true;
				}
				else{
					leds_off(LED_GREEN);
					leds_off(LED_RED);
					leds_off(LED_ORANGE);
					on = false;
				}
				
				
				//send an ack
				//acknowledgement.src = NODE_ID;
				//acknowledgement.dst = newPkt.src;
				//acknowledgement.ack = 1; //1 for true.
				
				//canSend = true;
			}
			else{
				//number given is too big. Probably also an imposter.
				printf("Number too big, don't open the gate!\r\n\r\n");
				leds_off(LED_GREEN);
				leds_off(LED_RED);
				leds_on(LED_ORANGE);
			}
		}
	}
}

void application_radio_tx_done()
{
	tx_buffer_inuse = false;
	
	//empty buffers after data has been sent
	for(int j=0; j<RADIO_MAX_LEN; j++)
	{
		tx_buffer[j] = 0x00;
	}
}

void application_button_pressed(){
	//Not needed.
}

void application_button_released(){
	//Not needed.
}