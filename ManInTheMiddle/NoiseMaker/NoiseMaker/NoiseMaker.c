//
// AVR C library
//
#include <avr/io.h>
//
// Standard C include files
//
#include <stdbool.h>
#include <stdio.h>
#include <stdio.h>
//
// You MUST include app.h and implement every function declared
//
#include "app.h"
#include "string.h"
//
// Include the header files for the various required libraries
//
#include "simple_os.h"
#include "button.h"
#include "leds.h"
#include "radio.h"
#include "serial.h"
#include "hw_timer.h"

//unsigned short gate = 0x02;
//
// Constants
//
typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	uint16_t num;
	uint16_t ender;
} Packet;

Packet newPkt;
//uint16_t num;
char str[8];
uint16_t gate;

typedef struct Req {
	uint16_t dst;
	uint16_t src;
	uint16_t req;
} Req;

Req request;
//
// Global Variables
//
static timer timer1;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Check false and set to true before sending a message. Set to false in tx_done

//
// App init function
//

void application_start()
{
	leds_init();
	button_init();
	radio_init(NODE_ID, true); //true indicates receives radio message for ALL nodes
	radio_set_power(1);
	radio_start();
	serial_init(9600);
	//printf("test\r\n");
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 100);
	timer_start(&timer1);
}
//
// Timer tick handler
//

//Waiting for a "should I open?" messsage from the gate. When picked up, send it along to its destination, and wait for the response.
void application_timer_tick(timer *t)
{
	for(int i=0;i<8;i++){
		str[i] = 0;
	}
	int ind = 0;
	
	 while(serial_ready()){
		 leds_on(LED_RED);
		 str[ind] = serial_get();
		 if(str[ind] == 'Y'){
			canSend = true;
			break;
		 } 
		 ind++;
	 }
	 
	if(canSend){
		canSend = false;
		
		request.dst = str[0];
		request.src = str[2];
		request.req = str[4];
		gate = request.src;
		
		//send on that packet to its destination, and pick up the response
		leds_on(LED_ORANGE);
		if(tx_buffer_inuse == false)
		{
			tx_buffer_inuse = true;
				 
			memcpy(&tx_buffer, &request, sizeof(Req));
				 
			radio_send(tx_buffer, sizeof(Req), request.dst);
		}
	}	
	
			 
	 /*while(serial_ready()){
		 str[ind] = serial_get();
		 ind++;
		 
		 uint16_t r = str[4];
		 leds_on(LED_RED);
		 if(r == 1){ //its an open message
			 //Turn it into a packet
			 request.dst = str[0];
			 request.src = str[2];
			 request.req = str[4];
			 gate = request.src;
			 
			 //send on that packet to its destination, and pick up the response
			 leds_on(LED_ORANGE);
			 if(tx_buffer_inuse == false)
			 {
				 tx_buffer_inuse = true;
				 
				 memcpy(&tx_buffer, &request, sizeof(Req));
				 
				 radio_send(tx_buffer, sizeof(Req), request.dst);
			 }
			 break;
		 }
	 } */
}

//
// This function is called whenever a radio message is received
// You must copy any data you need out of the packet - as 'msgdata' will be overwritten by the next message
//
void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{
	newPkt.dst = msgdata[0];
	newPkt.src = msgdata[2];
	newPkt.num = msgdata[4];
	
	if(newPkt.dst == gate){
		//save the packet, send it on to attacker
		
		leds_on(LED_GREEN);
		unsigned char buf[8];
		sprintf(buf, "%02d%02d%02d%c", newPkt.dst, newPkt.src, newPkt.num, 'Y');
		
		//unsigned char buf[8];
		//newPkt.ender = 27;
		//memcpy(&buf, &newPkt, sizeof(Packet));
		serial_puts(buf);
	}
}

//
// This function is called whenever a radio message has been transmitted
// You need to free up the transmit buffer here
//
void application_radio_tx_done()
{
	tx_buffer_inuse = false;
	
	//empty buffers after data has been sent
	for(int j=0; j<RADIO_MAX_LEN; j++)
	{
		tx_buffer[j] = 0x00;
	}
}

void application_button_pressed()
{
	
}

void application_button_released()
{
	
}