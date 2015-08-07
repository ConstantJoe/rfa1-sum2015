/*
Joseph Finnegan
Maynooth University
Summer 2015

For use with an ATmega128RFA1 microcontroller
Works with an opener and an attacker, to show that an attacker can imitate an opener easily if messages are not encrypted.

On button press, sends an OPEN request to a partnered gate.
When it receives a packet:
If that packet contained a number, it increments it, and sends it back to the gate.
If that packet contained an ack or nack, it knows if its open attempt has been successful or not.

Since none of its messages are encrypted, an attacker can listen in and learn the protocol for opening the gate (in this case, increment the received number and send it back)
*/

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
#include "string.h"

//
// Header files for the various required libraries
//
#include "app.h"
#include "simple_os.h"
#include "button.h"
#include "leds.h"
#include "radio.h"
#include "serial.h"
#include "hw_timer.h"

//
// Constants
//
#define OPEN "OPENOPENOPENOPEN"
#define ACK "ACKACKacACKACKac"
#define NACK "NACKNACKNACKNACK"

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

	
	if(strcmp(string, NACK) == 0){
		//Uh oh, our last attempt at opening the gate failed.
		leds_off(LED_GREEN);
		leds_off(LED_ORANGE);
		leds_on(LED_RED);
	}
	else if(strcmp(string, ACK) == 0){
		//Gate has opened.
		leds_off(LED_RED);
		leds_off(LED_ORANGE);
		leds_on(LED_GREEN);
	}
	else{
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
	sprintf(newPkt.dt, OPEN);
	canSend = true;
	
	leds_off(LED_RED);
	leds_on(LED_ORANGE);
	leds_off(LED_GREEN);
}

void application_button_released()
{
	//not needed
}