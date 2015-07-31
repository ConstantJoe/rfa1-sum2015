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

uint16_t num = 15;
Packet newPkt;
Req request;
static timer timer1;
bool canSend = false;
uint16_t gate = 0x02;
bool open = false;

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
	radio_init(NODE_ID, false);
	radio_set_power(1);
	radio_start();
	serial_init(9600);
	printf("test\r\n");
	
	timer_init(&timer1, TIMER_MILLISECONDS, 1000, 100);
	timer_start(&timer1);
}
//
// Timer tick handler
//
void application_timer_tick(timer *t)
{
	if(canSend){
		if(tx_buffer_inuse == false)
		{
			tx_buffer_inuse = true;
			canSend = false;
			
			memcpy(&tx_buffer, &newPkt, sizeof(Packet));
			
			radio_send(tx_buffer, sizeof(Packet), newPkt.dst);
		}
	}
	
}

//
// This function is called whenever a radio message is received
// You must copy any data you need out of the packet - as 'msgdata' will be overwritten by the next message
//
void application_radio_rx_msg(unsigned short dst, unsigned short src, int len, unsigned char *msgdata)
{
	request.dst = msgdata[0];
	request.src = msgdata[2];
	request.req = msgdata[4];
	
	if(request.req == 1){
		//sweet, the gate opened.
		/*if(!open){
			leds_on(LED_GREEN);
			leds_off(LED_RED);
			leds_off(LED_ORANGE);
			open = true;
		}
		else{
			leds_off(LED_GREEN);
			leds_off(LED_RED);
			leds_off(LED_ORANGE);
			open = false;
		}*/
		newPkt.dst = request.src;
		newPkt.src = NODE_ID;
		newPkt.num = num;
		canSend = true;
		num++;
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
	newPkt.dst = gate;
	newPkt.src = NODE_ID;
	newPkt.num = num;
	canSend = true;
	num++;
}

void application_button_released()
{
	
}