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

//
// Constants
//
typedef struct Packet {
	uint16_t dst;
	uint16_t src;
	//uint64_t data;
	char dt[16];
} Packet;


//
// Global Variables
//
static timer timer1;

// Buffer for transmitting radio packets
unsigned char tx_buffer[RADIO_MAX_LEN];
bool tx_buffer_inuse=false; // Check false and set to true before sending a message. Set to false in tx_done

int dest = 0x02;
bool canSend = false;
Packet newPkt;

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
	printf("received a message\r\n");
	char string[17];
	for(int i = 4; i<len; i++)
	{
		printf("%c", msgdata[i]);
		string[i-4] = msgdata[i];
	}
	string[16] = '\0';

	//put string into plaintext
	//sprintf(plaintext, string);
	//decrypt plaintext
	//decryptMeth();
	
	if(strcmp(string, "NACKNACKNACKNACK") == 0){ //something more complex than this - should be encrypted too?
		//Uh oh.
		leds_off(LED_GREEN);
		leds_off(LED_ORANGE);
		leds_on(LED_RED);
	}
	else if(strcmp(string, "ACKACKacACKACKac") == 0){ //something more complex than this.
		//Gate has opened.
		leds_off(LED_RED);
		leds_off(LED_ORANGE);
		leds_on(LED_GREEN);
	}
	else{
		//for the purposes of this demo, the opener won't give a response. The attacker will steal the number and give a response instead.
		long temp1 = atol(string); //should be atolu?
		temp1++;

		newPkt.dst = src;
		newPkt.src = NODE_ID;
		sprintf(newPkt.dt, "%lu", temp1);
		canSend = true;
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
	newPkt.dst = 0x00;
	newPkt.src = 0x00;
	for(int i=0;i<sizeof(newPkt.dt);i++){
		newPkt.dt[i] = 0;
	}
}

void application_button_pressed()
{
	newPkt.dst = dest;
	newPkt.src = NODE_ID;
	sprintf(newPkt.dt, "OPENOPENOPENOPEN");
	canSend = true;
	
	leds_off(LED_RED);
	leds_on(LED_ORANGE);
	leds_off(LED_GREEN);
}

void application_button_released()
{
	
}